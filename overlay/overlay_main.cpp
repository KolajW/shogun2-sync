#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdbool.h>

#include <stdint.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "minhook/include/MinHook.h"
#include "../kw_shogun.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT (WINAPI *Present9_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT (WINAPI *Reset9_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT (WINAPI *Present11_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT (WINAPI *ResizeBuffers11_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

static Present9_t oPresent9 = NULL;
static Reset9_t oReset9 = NULL;
static Present11_t oPresent11 = NULL;
static ResizeBuffers11_t oResizeBuffers11 = NULL;
static WNDPROC oWndProc = NULL;

static HANDLE hIPC = NULL;
static ShogunSharedData *pShared = NULL;

static bool g_bInitDX9 = false;
static bool g_bInitDX11 = false;
static HWND g_hGameWnd = NULL;
static bool g_bInteractive = false;
static DWORD g_dwReloadNotificationExpire = 0;
static char g_szReloadNotification[128] = "";

static ID3D11Device *g_pDevice11 = NULL;
static ID3D11DeviceContext *g_pContext11 = NULL;
static ID3D11RenderTargetView *g_pRenderTargetView11 = NULL;

#define WM_SHOGUN_RELOAD (WM_USER + 0x777)

struct WarscapeString {
    uint32_t length;
    uint32_t capacity;
    const wchar_t *buffer;
};

static bool IsValidReadPtr(const void *ptr, size_t size) {
    if (!ptr) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    uintptr_t end = (uintptr_t)ptr + size;
    uintptr_t pageEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (end > pageEnd) {
        if (VirtualQuery((LPCVOID)pageEnd, &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    }
    return true;
}

typedef void (__attribute__((thiscall)) *QueueCampaignLoad_t)(void *pCampMgr, void *pPathObj, unsigned int flags);
typedef void* (__cdecl *LoadPlayersSetup_t)(void *arg1, void *arg2);
typedef void (__cdecl *QuickLoad_t)(void);

static LoadPlayersSetup_t oLoadPlayersSetup = NULL;
static QuickLoad_t oQuickLoad = NULL;
static uint8_t g_savedGuestCamera[16] = {0};
static bool g_hasSavedGuestCamera = false;
static bool g_bPendingReloadInvocation = false;

static void TriggerSaveReload(const char *saveName);

static void __cdecl Hook_QuickLoad(void) {
    if (pShared != NULL && pShared->room_code[0] != '\0') {
        TriggerSaveReload("autosave.save_multiplayer");
        return;
    }
    HMODULE hEngine = GetModuleHandleA("empire.retail.dll");
    if (hEngine) {
        uintptr_t baseAddr = (uintptr_t)hEngine;
        uintptr_t pAppPtr = baseAddr + 0x01BC8FE0;
        if (IsValidReadPtr((const void*)pAppPtr, sizeof(void*))) {
            uintptr_t pApp = *(uintptr_t*)pAppPtr;
            if (pApp && IsValidReadPtr((const void*)(pApp + 0x0A98), sizeof(void*))) {
                uintptr_t pCampMgr = *(uintptr_t*)(pApp + 0x0A98);
                if (pCampMgr && IsValidReadPtr((const void*)(pCampMgr + 0x74), sizeof(void*))) {
                    void *pSession = *(void**)(pCampMgr + 0x74);
                    if (pSession != NULL) {
                        TriggerSaveReload("autosave.save_multiplayer");
                        return;
                    }
                }
            }
        }
    }
    if (oQuickLoad) {
        oQuickLoad();
    }
}

static void* __cdecl Hook_LoadPlayersSetup(void *arg1, void *arg2) {
    void *retVal = oLoadPlayersSetup ? oLoadPlayersSetup(arg1, arg2) : NULL;
    void *candidates[2] = { arg1, arg2 };
    for (int i = 0; i < 2; i++) {
        uint8_t *vec = (uint8_t*)candidates[i];
        if (!IsValidReadPtr(vec, 12)) continue;
        uint32_t capacity = *(uint32_t*)vec;
        uint32_t count = *(uint32_t*)(vec + 4);
        uint8_t *data = *(uint8_t**)(vec + 8);
        if (count >= 2 && count <= 32 && capacity >= count && data != NULL) {
            size_t reqBytes = (size_t)count * 0xb8;
            if (IsValidReadPtr(data, reqBytes)) {
                DWORD oldProtect = 0;
                if (VirtualProtect(data, reqBytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    uint8_t *p0 = data;
                    uint8_t *p1 = data + 0xb8;
                    bool isLocalHost = (pShared != NULL && pShared->isHost);
                    if (isLocalHost) {
                        p0[0xa1] = 1;
                        p1[0xa1] = 0;
                    } else {
                        p0[0xa1] = 0;
                        p1[0xa1] = 1;
                        float *p1Cam = (float*)(p1 + 0xa4);
                        if (p1Cam[0] != 0.0f || p1Cam[1] != 0.0f || p1Cam[2] != 0.0f) {
                            memcpy(g_savedGuestCamera, p1 + 0xa4, 16);
                            g_hasSavedGuestCamera = true;
                            memcpy(p0 + 0xa4, p1 + 0xa4, 16);
                        } else if (g_hasSavedGuestCamera) {
                            memcpy(p0 + 0xa4, g_savedGuestCamera, 16);
                            memcpy(p1 + 0xa4, g_savedGuestCamera, 16);
                        } else {
                            float dateCam[4] = { 240.0f, 180.0f, 400.0f, 0.0f };
                            memcpy(g_savedGuestCamera, dateCam, 16);
                            g_hasSavedGuestCamera = true;
                            memcpy(p0 + 0xa4, dateCam, 16);
                            memcpy(p1 + 0xa4, dateCam, 16);
                        }
                    }
                    if (oldProtect != 0) {
                        VirtualProtect(data, reqBytes, oldProtect, &oldProtect);
                    }
                }
            }
        }
    }
    return retVal;
}

static bool ExecuteEngineQueueLoad(const char *saveName) {
    const char *sName = (saveName != NULL && saveName[0] != '\0') ? saveName : "autosave.save_multiplayer";
    bool isMultiplayer = (pShared != NULL && pShared->room_code[0] != '\0') || (strstr(sName, "multiplayer") != NULL);

    HMODULE hEngine = GetModuleHandleA("empire.retail.dll");
    if (!hEngine) {
        snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "empire.retail.dll not found in process");
        g_dwReloadNotificationExpire = GetTickCount() + 6000;
        return false;
    }

    uintptr_t baseAddr = (uintptr_t)hEngine;
    uintptr_t pAppPtr = baseAddr + 0x01BC8FE0;
    if (!IsValidReadPtr((const void*)pAppPtr, sizeof(void*))) {
        snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Application singleton pointer unreadable");
        g_dwReloadNotificationExpire = GetTickCount() + 6000;
        return false;
    }

    uintptr_t pApp = *(uintptr_t*)pAppPtr;
    if (!pApp || !IsValidReadPtr((const void*)pApp, 0x0A9C)) {
        snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Application singleton not initialized");
        g_dwReloadNotificationExpire = GetTickCount() + 6000;
        return false;
    }

    uintptr_t pCampMgr = *(uintptr_t*)(pApp + 0x0A98);
    if (!pCampMgr || !IsValidReadPtr((const void*)pCampMgr, 0x100)) {
        snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Campaign not active (pCampMgr is null)");
        g_dwReloadNotificationExpire = GetTickCount() + 6000;
        return false;
    }

    void *pSession = *(void**)(pCampMgr + 0x74);
    if (pSession != NULL) {
        isMultiplayer = true;
    }

    if (isMultiplayer) {
        snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "[ SAVE SYNCED ] Save synced across peers. Load in lobby to continue.");
        g_dwReloadNotificationExpire = GetTickCount() + 8000;
        if (pShared != NULL) {
            snprintf(pShared->status_msg, sizeof(pShared->status_msg), "Save synced to disk. Rehost/load in lobby to apply.");
            pShared->status = SYNC_STATUS_OK;
        }
        return true;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect((LPVOID)pCampMgr, 0x100, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Failed to unprotect campaign manager memory");
        g_dwReloadNotificationExpire = GetTickCount() + 6000;
        return false;
    }

    QueueCampaignLoad_t fnQueueLoad = (QueueCampaignLoad_t)(baseAddr + 0x006CBA70);

    wchar_t wSaveName[128];
    MultiByteToWideChar(CP_UTF8, 0, sName, -1, wSaveName, 128);

    WarscapeString strObj;
    strObj.length = (uint32_t)wcslen(wSaveName);
    strObj.capacity = strObj.length;
    strObj.buffer = wSaveName;

    fnQueueLoad((void*)pCampMgr, &strObj, 0);

    if (oldProtect != 0) {
        VirtualProtect((LPVOID)pCampMgr, 0x100, oldProtect, &oldProtect);
    }

    snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Campaign reload queued in Warscape Engine!");
    g_dwReloadNotificationExpire = GetTickCount() + 6000;
    if (pShared != NULL) {
        snprintf(pShared->status_msg, sizeof(pShared->status_msg), "Save reloaded in Warscape Engine.");
    }
    return true;
}

static void CheckIPC(){
    if (pShared != NULL) {
        return;
    }
    if (hIPC == NULL) {
        hIPC = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Local\\ShogunSyncIPC");
        if (hIPC == NULL) {
            hIPC = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(ShogunSharedData), "Local\\ShogunSyncIPC");
        }
    }
    if (hIPC != NULL && pShared == NULL) {
        pShared = (ShogunSharedData *)MapViewOfFile(hIPC, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShogunSharedData));
        if (pShared == NULL) {
            CloseHandle(hIPC);
            hIPC = NULL;
        }
    }
}

static void TriggerSaveReload(const char *saveName){
    CheckIPC();
    DWORD now = GetTickCount();
    if (pShared != NULL) {
        if (now - pShared->lastReloadTick < 3000) {
            return;
        }
        pShared->lastReloadTick = now;
        pShared->reloadCount++;
        if (saveName != NULL && saveName[0] != '\0') {
            strncpy(pShared->lastSaveName, saveName, sizeof(pShared->lastSaveName) - 1);
            pShared->lastSaveName[sizeof(pShared->lastSaveName) - 1] = '\0';
        }
        if (pShared->room_code[0] != '\0') {
            pShared->reqSync = true;
            snprintf(pShared->status_msg, sizeof(pShared->status_msg), "Sync requested across multiplayer peers.");
            snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Sync requested across multiplayer peers.");
            g_dwReloadNotificationExpire = GetTickCount() + 6000;
        } else {
            g_bPendingReloadInvocation = true;
        }
    } else {
        g_bPendingReloadInvocation = true;
    }
}

static void ProcessPendingReload(){
    CheckIPC();
    if (g_bPendingReloadInvocation || (pShared != NULL && pShared->triggerReload)) {
        g_bPendingReloadInvocation = false;
        const char *sName = (pShared != NULL && pShared->room_code[0] != '\0') ? "autosave.save_multiplayer" : "auto_save.save";
        if (pShared != NULL) {
            pShared->triggerReload = false;
            if (pShared->lastSaveName[0] != '\0') {
                sName = pShared->lastSaveName;
            }
        }
        if (g_hGameWnd) {
            PostMessageA(g_hGameWnd, WM_SHOGUN_RELOAD, 0, 0);
        } else {
            ExecuteEngineQueueLoad(sName);
        }
    }
}

static LRESULT WINAPI Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    if (uMsg == WM_SHOGUN_RELOAD) {
        const char *sName = (pShared != NULL && pShared->room_code[0] != '\0') ? "autosave.save_multiplayer" : "auto_save.save";
        if (pShared != NULL && pShared->lastSaveName[0] != '\0') {
            sName = pShared->lastSaveName;
        }
        ExecuteEngineQueueLoad(sName);
        return 0;
    }
    if (wParam == VK_F10) {
        if (uMsg == WM_KEYDOWN) {
            g_bInteractive = !g_bInteractive;
        }
        return 0;
    }
    if (wParam == VK_F9) {
        if (uMsg == WM_KEYDOWN) {
            if (pShared != NULL && pShared->room_code[0] != '\0') {
                TriggerSaveReload("autosave.save_multiplayer");
            } else {
                TriggerSaveReload("auto_save.save");
            }
        }
        return 0;
    }

    if (g_bInteractive) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
            return true;
        }
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            return true;
        }
    }

    return CallWindowProcA(oWndProc, hWnd, uMsg, wParam, lParam);
}

static void RenderHUD(){
    CheckIPC();

    DWORD now = GetTickCount();
    bool bActive = (pShared != NULL && !pShared->shouldExit && (now - pShared->lastHeartbeatTick <= 2500));

    ImGui::SetNextWindowPos(ImVec2(16.0f, 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_AlwaysAutoResize;

    if (!g_bInteractive) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.28f, 0.90f));

    ImGui::Begin("ShogunSyncHUD", NULL, flags);

    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.25f, 1.0f), "Shogun-Sync");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "|");
    ImGui::SameLine();

    if (bActive) {
        ImGui::Text("Tour: %u", pShared->tour);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "|");
        ImGui::SameLine();

        switch (pShared->status) {
            case SYNC_STATUS_OK:
                ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.35f, 1.0f), "[ SYNC OK ]");
                break;
            case SYNC_STATUS_DESYNC:
                ImGui::TextColored(ImVec4(1.00f, 0.25f, 0.25f, 1.0f), "[ DESYNC DETECTED ]");
                break;
            case SYNC_STATUS_RESYNCING:
                ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.20f, 1.0f), "[ RESYNCING ]");
                break;
            case SYNC_STATUS_CONNECTING:
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.0f), "[ CONNECTING ]");
                break;
            default:
                ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.70f, 1.0f), "[ STANDBY ]");
                break;
        }

        if (pShared->room_code[0] != '\0') {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "|");
            ImGui::SameLine();
            ImGui::Text("Room: %s (%s)", pShared->room_code, pShared->isHost ? "Host" : "Client");
        }

        if (pShared->lobbyMaxUsers > 0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "|");
            ImGui::SameLine();
            if (pShared->lobbyUsers >= pShared->lobbyMaxUsers) {
                ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.35f, 1.0f), "Lobby: %u/%u", pShared->lobbyUsers, pShared->lobbyMaxUsers);
            } else {
                ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.20f, 1.0f), "Lobby: %u/%u", pShared->lobbyUsers, pShared->lobbyMaxUsers);
            }
        }

        if (pShared->reloadCount > 0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "|");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.30f, 0.80f, 0.95f, 1.0f), "Sync Dispatches: %u", pShared->reloadCount);
        }
    } else {
        ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.70f, 1.0f), "[ Standby / Waiting for App ]");
    }

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "|");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.60f, 1.0f), "by KW");

    ImGui::End();

    if (GetTickCount() < g_dwReloadNotificationExpire) {
        ImGui::SetNextWindowPos(ImVec2(16.0f, 48.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.90f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.22f, 0.12f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.80f, 0.40f, 0.90f));
        ImGui::Begin("ReloadBanner", NULL, flags);
        ImGui::TextColored(ImVec4(0.30f, 0.95f, 0.45f, 1.0f), "[ NOTIFICATION ] %s", g_szReloadNotification);
        ImGui::End();
        ImGui::PopStyleColor(2);
    }

    if (g_bInteractive) {
        ImGui::SetNextWindowPos(ImVec2(16.0f, 86.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 170.0f), ImGuiCond_Always);
        ImGui::Begin("Shogun-Sync Menu [F10 to Close]", &g_bInteractive, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        if (ImGui::Button("Sync Save to Disk (F9)", ImVec2(310.0f, 32.0f))) {
            const char *name = (pShared != NULL && pShared->lastSaveName[0] != '\0') ? pShared->lastSaveName : "autosave.save_multiplayer";
            TriggerSaveReload(name);
        }

        ImGui::Spacing();
        if (bActive) {
            ImGui::Text("Last Save: %s", pShared->lastSaveName[0] != '\0' ? pShared->lastSaveName : "None");
            ImGui::Text("Sync Dispatches: %u", pShared->reloadCount);
        } else {
            ImGui::TextDisabled("Status: Standby (App not connected)");
        }
        ImGui::TextDisabled("Hotkeys: F9 = Sync Save | F10 = Menu");
        ImGui::TextDisabled("In-game multiplayer hot-reload active.");

        ImGui::End();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

static HRESULT WINAPI Hook_Present9(IDirect3DDevice9 *pDevice, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion){
    if (!g_bInitDX9) {
        D3DDEVICE_CREATION_PARAMETERS params;
        if (SUCCEEDED(pDevice->GetCreationParameters(&params))) {
            g_hGameWnd = params.hFocusWindow;
        }
        if (!g_hGameWnd) {
            g_hGameWnd = GetActiveWindow();
        }

        if (g_hGameWnd && !oWndProc) {
            oWndProc = (WNDPROC)SetWindowLongPtrA(g_hGameWnd, GWLP_WNDPROC, (LONG_PTR)Hook_WndProc);
        }

        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = NULL;
        io.LogFilename = NULL;

        ImGui_ImplWin32_Init(g_hGameWnd);
        ImGui_ImplDX9_Init(pDevice);
        g_bInitDX9 = true;
    }

    ProcessPendingReload();

    if (pDevice->TestCooperativeLevel() != D3D_OK) {
        return oPresent9(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderHUD();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oPresent9(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

static HRESULT WINAPI Hook_Reset9(IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters){
    if (g_bInitDX9) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
    HRESULT hr = oReset9(pDevice, pPresentationParameters);
    if (g_bInitDX9 && SUCCEEDED(hr)) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return hr;
}

static HRESULT WINAPI Hook_Present11(IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags){
    if (!g_bInitDX11) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pDevice11))) {
            g_pDevice11->GetImmediateContext(&g_pContext11);
            DXGI_SWAP_CHAIN_DESC desc;
            pSwapChain->GetDesc(&desc);
            g_hGameWnd = desc.OutputWindow;
            if (!g_hGameWnd) {
                g_hGameWnd = GetActiveWindow();
            }

            if (g_hGameWnd && !oWndProc) {
                oWndProc = (WNDPROC)SetWindowLongPtrA(g_hGameWnd, GWLP_WNDPROC, (LONG_PTR)Hook_WndProc);
            }

            ID3D11Texture2D *pBackBuffer = NULL;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
            if (pBackBuffer) {
                g_pDevice11->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView11);
                pBackBuffer->Release();
            }

            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            io.IniFilename = NULL;
            io.LogFilename = NULL;

            ImGui_ImplWin32_Init(g_hGameWnd);
            ImGui_ImplDX11_Init(g_pDevice11, g_pContext11);
            g_bInitDX11 = true;
        }
    }

    ProcessPendingReload();

    if (g_bInitDX11 && !g_pRenderTargetView11 && g_pDevice11) {
        ID3D11Texture2D *pBackBuffer = NULL;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (pBackBuffer) {
            g_pDevice11->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView11);
            pBackBuffer->Release();
        }
    }

    if (g_bInitDX11 && g_pRenderTargetView11) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderHUD();

        ImGui::EndFrame();
        ImGui::Render();
        g_pContext11->OMSetRenderTargets(1, &g_pRenderTargetView11, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return oPresent11(pSwapChain, SyncInterval, Flags);
}

static HRESULT WINAPI Hook_ResizeBuffers11(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags){
    if (g_pRenderTargetView11) {
        g_pRenderTargetView11->Release();
        g_pRenderTargetView11 = NULL;
    }
    HRESULT hr = oResizeBuffers11(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    if (g_pDevice11) {
        ID3D11Texture2D *pBackBuffer = NULL;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (pBackBuffer) {
            g_pDevice11->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView11);
            pBackBuffer->Release();
        }
    }
    return hr;
}

static HWND CreateDummyWindow(){
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, DefWindowProcA, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX_DUMMY", NULL };
    RegisterClassExA(&wc);
    return CreateWindowExA(0, "DX_DUMMY", "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
}

static void DestroyDummyWindow(HWND hWnd){
    DestroyWindow(hWnd);
    UnregisterClassA("DX_DUMMY", GetModuleHandleA(NULL));
}

static bool GetD3D9VTable(void **vtable, size_t size){
    HMODULE hD3D9 = GetModuleHandleA("d3d9.dll");
    if (!hD3D9) {
        hD3D9 = LoadLibraryA("d3d9.dll");
        if (!hD3D9) return false;
    }

    typedef IDirect3D9* (WINAPI *pDirect3DCreate9)(UINT);
    pDirect3DCreate9 fnDirect3DCreate9 = (pDirect3DCreate9)GetProcAddress(hD3D9, "Direct3DCreate9");
    if (!fnDirect3DCreate9) return false;

    IDirect3D9 *pD3D = fnDirect3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return false;

    HWND hWnd = CreateDummyWindow();
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;

    IDirect3DDevice9 *pDevice = NULL;
    HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice);
    if (FAILED(hr)) {
        hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice);
    }

    if (SUCCEEDED(hr) && pDevice) {
        memcpy(vtable, *(void***)pDevice, size);
        pDevice->Release();
        pD3D->Release();
        DestroyDummyWindow(hWnd);
        return true;
    }

    pD3D->Release();
    DestroyDummyWindow(hWnd);
    return false;
}

static bool GetD3D11VTable(void **vtable, size_t size){
    HMODULE hD3D11 = GetModuleHandleA("d3d11.dll");
    if (!hD3D11) {
        hD3D11 = LoadLibraryA("d3d11.dll");
        if (!hD3D11) return false;
    }

    typedef HRESULT (WINAPI *pD3D11CreateDeviceAndSwapChain)(
        IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
        const D3D_FEATURE_LEVEL*, UINT, UINT,
        const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
        ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**
    );

    pD3D11CreateDeviceAndSwapChain fnCreate = (pD3D11CreateDeviceAndSwapChain)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
    if (!fnCreate) return false;

    HWND hWnd = CreateDummyWindow();
    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    IDXGISwapChain *pSwapChain = NULL;
    ID3D11Device *pDevice = NULL;
    ID3D11DeviceContext *pContext = NULL;

    HRESULT hr = fnCreate(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 2, D3D11_SDK_VERSION, &scd, &pSwapChain, &pDevice, &featureLevel, &pContext);
    if (FAILED(hr)) {
        hr = fnCreate(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, featureLevels, 2, D3D11_SDK_VERSION, &scd, &pSwapChain, &pDevice, &featureLevel, &pContext);
    }

    if (SUCCEEDED(hr) && pSwapChain) {
        memcpy(vtable, *(void***)pSwapChain, size);
        pSwapChain->Release();
        if (pContext) pContext->Release();
        if (pDevice) pDevice->Release();
        DestroyDummyWindow(hWnd);
        return true;
    }

    DestroyDummyWindow(hWnd);
    return false;
}

static DWORD WINAPI InitOverlayThread(LPVOID lpParam){
    Sleep(500);

    CheckIPC();

    MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        return 1;
    }

    void *vtable9[119];
    if (GetD3D9VTable(vtable9, sizeof(vtable9))) {
        MH_CreateHook(vtable9[17], (void*)Hook_Present9, (void**)&oPresent9);
        MH_CreateHook(vtable9[16], (void*)Hook_Reset9, (void**)&oReset9);
    }

    void *vtable11[18];
    if (GetD3D11VTable(vtable11, sizeof(vtable11))) {
        MH_CreateHook(vtable11[8], (void*)Hook_Present11, (void**)&oPresent11);
        MH_CreateHook(vtable11[13], (void*)Hook_ResizeBuffers11, (void**)&oResizeBuffers11);
    }

    HMODULE hEngine = GetModuleHandleA("empire.retail.dll");
    if (hEngine) {
        uintptr_t baseAddr = (uintptr_t)hEngine;
        void *pLoadPlayers = (void*)(baseAddr + 0x0057FE40);
        MH_CreateHook(pLoadPlayers, (void*)Hook_LoadPlayersSetup, (void**)&oLoadPlayersSetup);
        void *pQuickLoad = (void*)(baseAddr + 0x0117A730);
        MH_CreateHook(pQuickLoad, (void*)Hook_QuickLoad, (void**)&oQuickLoad);
    }

    MH_EnableHook(MH_ALL_HOOKS);

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved){
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            CreateThread(NULL, 0, InitOverlayThread, NULL, 0, NULL);
            break;
        case DLL_PROCESS_DETACH:
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            if (oWndProc && g_hGameWnd) {
                SetWindowLongPtrA(g_hGameWnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);
                oWndProc = NULL;
            }
            if (pShared != NULL) {
                UnmapViewOfFile(pShared);
                pShared = NULL;
            }
            if (hIPC != NULL) {
                CloseHandle(hIPC);
                hIPC = NULL;
            }
            break;
    }
    return TRUE;
}
