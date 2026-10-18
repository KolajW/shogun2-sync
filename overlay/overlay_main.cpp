#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdbool.h>

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

static void CheckIPC(){
    if (pShared != NULL) {
        return;
    }
    if (hIPC == NULL) {
        hIPC = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Local\\ShogunSyncIPC");
    }
    if (hIPC != NULL) {
        pShared = (ShogunSharedData *)MapViewOfFile(hIPC, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShogunSharedData));
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
        pShared->triggerReload = true;
        snprintf(pShared->status_msg, sizeof(pShared->status_msg), "Save synced to disk. Rehost/load in lobby to apply.");
    }

    g_dwReloadNotificationExpire = now + 6000;
    snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Save synced to disk. Rehost/load in lobby to apply.");
}

static LRESULT WINAPI Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    if (uMsg == WM_KEYDOWN && wParam == VK_F10) {
        g_bInteractive = !g_bInteractive;
        return 0;
    }
    if (uMsg == WM_KEYDOWN && wParam == VK_F9) {
        TriggerSaveReload("autosave.save_multiplayer");
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
    if (pShared == NULL || pShared->shouldExit || (now - pShared->lastHeartbeatTick > 2500)) {
        return;
    }

    if (pShared->triggerReload) {
        DWORD now = GetTickCount();
        g_dwReloadNotificationExpire = now + 6000;
        snprintf(g_szReloadNotification, sizeof(g_szReloadNotification), "Save synced to disk. Rehost/load in lobby to apply.");
    }

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

    if (pShared != NULL) {
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
        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Waiting for shogun-sync...");
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
        ImGui::TextColored(ImVec4(0.30f, 0.95f, 0.45f, 1.0f), "[ SAVE SYNCED ] %s", g_szReloadNotification);
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
        if (pShared != NULL) {
            ImGui::Text("Last Save: %s", pShared->lastSaveName[0] != '\0' ? pShared->lastSaveName : "None");
            ImGui::Text("Sync Dispatches: %u", pShared->reloadCount);
        }
        ImGui::TextDisabled("Hotkeys: F9 = Sync Save | F10 = Menu");
        ImGui::TextDisabled("Rehost/load in multiplayer lobby to apply.");

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

    if (MH_Initialize() != MH_OK) {
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
