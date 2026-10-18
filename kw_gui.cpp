#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "overlay/imgui/imgui.h"
#include "overlay/imgui/backends/imgui_impl_win32.h"
#include "overlay/imgui/backends/imgui_impl_dx11.h"

#include "kw_gui.h"

extern "C" {
#include "kw_conf.h"
#include "kw_conn.h"
#include "kw_inject.h"
#include "kw_update.h"
#include "kw_shogun.h"
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device *g_pd3dDevice = NULL;
static ID3D11DeviceContext *g_pd3dDeviceContext = NULL;
static IDXGISwapChain *g_pSwapChain = NULL;
static ID3D11RenderTargetView *g_mainRenderTargetView = NULL;

static char g_inputPass[64] = "";
static char g_inputRoom[16] = "";
static int g_selectedMode = 0;
static HANDLE g_hSessionThread = NULL;
static char g_guiLog[8192] = "";

extern "C" void GuiLog(const char *text){
    if (strlen(g_guiLog) + strlen(text) + 2 < sizeof(g_guiLog)) {
        strcat(g_guiLog, text);
        strcat(g_guiLog, "\n");
    } else {
        g_guiLog[0] = '\0';
        strcat(g_guiLog, text);
        strcat(g_guiLog, "\n");
    }
}

static DWORD WINAPI SessionWorker(LPVOID lpParam){
    running = true;
    DWORD sPid = findShogunProcess();
    if (sPid != 0) {
        if (autoInjectShogun("shogun_overlay32.dll") == 0) {
            GuiLog("Auto-inject: Overlay injected into Shogun 2");
        }
    }
    if (g_selectedMode == 0) {
        isHost = true;
        GuiLog("Initializing host session...");
        if (commit((char*)"GUI room commit") != 0) {
            GuiLog("Error 018: Commit creation failed");
            running = false;
            return 1;
        }

        srand((unsigned int)time(NULL));
        char roomCode[7];
        generateRoomCode(roomCode, sizeof(roomCode));
        strncpy(g_inputRoom, roomCode, sizeof(g_inputRoom) - 1);
        g_inputRoom[sizeof(g_inputRoom) - 1] = '\0';

        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "Room created. Code: %s", roomCode);
        GuiLog(logMsg);

        initIPC();
        if (shared != NULL) {
            strncpy(shared->room_code, roomCode, sizeof(shared->room_code) - 1);
            shared->room_code[sizeof(shared->room_code) - 1] = '\0';
            shared->isHost = true;
            shared->lobbyUsers = 1;
            shared->lobbyMaxUsers = 2;
            shared->status = SYNC_STATUS_CONNECTING;
            snprintf(shared->status_msg, sizeof(shared->status_msg), "Room created (%s). Waiting for partner...", roomCode);
        }

        mg_log_level = MG_LL_NONE;
        mg_log_set(MG_LL_NONE);
        connectToRoom(roomCode, g_inputPass, (char*)"create");
        connection = true;

        autoInjectShogun("shogun_overlay32.dll");

        watch();
    } else if (g_selectedMode == 1) {
        isHost = false;
        GuiLog("Connecting to room...");
        if (commit((char*)"GUI join commit") != 0) {
            GuiLog("Error 018: Commit creation failed");
            running = false;
            return 1;
        }

        char cleanRoom[16] = {0};
        size_t cLen = 0;
        for (size_t i = 0; g_inputRoom[i] != '\0' && cLen < sizeof(cleanRoom) - 1; i++) {
            if (g_inputRoom[i] != ' ' && g_inputRoom[i] != '\t' && g_inputRoom[i] != '\r' && g_inputRoom[i] != '\n') {
                cleanRoom[cLen++] = (char)toupper((unsigned char)g_inputRoom[i]);
            }
        }
        cleanRoom[cLen] = '\0';
        strncpy(g_inputRoom, cleanRoom, sizeof(g_inputRoom) - 1);
        g_inputRoom[sizeof(g_inputRoom) - 1] = '\0';

        initIPC();
        if (shared != NULL) {
            shared->isHost = false;
            shared->lobbyUsers = 0;
            shared->lobbyMaxUsers = 0;
            shared->room_code[0] = '\0';
            shared->status = SYNC_STATUS_CONNECTING;
            snprintf(shared->status_msg, sizeof(shared->status_msg), "Connecting to room %s...", cleanRoom);
        }

        mg_log_level = MG_LL_NONE;
        mg_log_set(MG_LL_NONE);
        connectToRoom(cleanRoom, g_inputPass, (char*)"join");
        connection = true;

        autoInjectShogun("shogun_overlay32.dll");

        watch();
    } else {
        GuiLog("Starting standalone watch...");
        initIPC();
        watch();
    }

    disconnectFromRoom();
    if (shared != NULL) {
        if (shared->status != SYNC_STATUS_DISCONNECTED) {
            shared->status = SYNC_STATUS_DISCONNECTED;
            snprintf(shared->status_msg, sizeof(shared->status_msg), "Session stopped");
        }
        shared->lobbyUsers = 0;
        shared->lobbyMaxUsers = 0;
        shared->room_code[0] = '\0';
    }
    GuiLog("Session stopped.");
    return 0;
}

static void CreateRenderTarget(){
    ID3D11Texture2D *pBackBuffer = NULL;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget(){
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = NULL;
    }
}

static bool CreateDeviceD3D(HWND hWnd){
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[4] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_1 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 4, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(res)) {
        res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 4, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    }
    if (FAILED(res)) {
        return false;
    }

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D(){
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) return 0;
            if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int launchGUI(void){
    HICON hIcon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1));
    WNDCLASSEXA wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleA(NULL), hIcon, LoadCursor(NULL, IDC_ARROW), NULL, NULL, "ShogunSyncGUI", hIcon };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Shogun Sync", WS_OVERLAPPEDWINDOW, 100, 100, 680, 560, NULL, NULL, wc.hInstance, NULL);

    if (hIcon != NULL) {
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool bRunningGUI = true;
    initIPC();
    if (shared != NULL) {
        shared->shouldExit = false;
        shared->status = SYNC_STATUS_DISCONNECTED;
        shared->tour = 0;
        shared->lastHeartbeatTick = GetTickCount();
        shared->lobbyUsers = 0;
        shared->lobbyMaxUsers = 0;
        shared->room_code[0] = '\0';
        snprintf(shared->status_msg, sizeof(shared->status_msg), "Ready");
    }
    autoInjectShogun("shogun_overlay32.dll");
    GuiLog("Shogun Sync GUI started. Ready.");

    while (bRunningGUI) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) {
                bRunningGUI = false;
            }
        }
        if (!bRunningGUI) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RECT rect;
        GetClientRect(hwnd, &rect);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top)));

        ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("MainWindow", NULL, winFlags);

        ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.25f, 1.0f), "SHOGUN SYNC");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "v0.1.3-beta | Total War: Shogun 2 Synchronization Tool");
        ImGui::Separator();

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Multiplayer Lobby")) {
                ImGui::Spacing();
                ImGui::Text("Session Mode:");
                ImGui::RadioButton("Host (Create Room)", &g_selectedMode, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Client (Join Room)", &g_selectedMode, 1);
                ImGui::SameLine();
                ImGui::RadioButton("Standalone Watch", &g_selectedMode, 2);

                ImGui::Spacing();

                if (g_selectedMode == 0) {
                    ImGui::InputText("Password", g_inputPass, sizeof(g_inputPass));
                    if (g_inputRoom[0] != '\0' && g_hSessionThread != NULL) {
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Active Room Code: %s", g_inputRoom);
                    }
                } else if (g_selectedMode == 1) {
                    ImGui::InputText("Room Code", g_inputRoom, sizeof(g_inputRoom));
                    ImGui::InputText("Password", g_inputPass, sizeof(g_inputPass));
                }

                ImGui::Spacing();

                bool bStorageInit = isStorageInitialized();
                if (!bStorageInit) {
                    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.2f, 1.0f), "Storage Not Initialized!");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "(Required before starting session)");
                    if (ImGui::Button("Initialize Storage Now (.sync)", ImVec2(240, 28))) {
                        if (init() == 0) {
                            GuiLog("Success: Storage initialized (.sync created)");
                        } else {
                            GuiLog("Error 007: Storage initialization failed");
                        }
                    }
                    ImGui::Spacing();
                } else {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Storage: Ready (.sync initialized)");
                    ImGui::Spacing();
                }

                if (g_hSessionThread != NULL) {
                    if (WaitForSingleObject(g_hSessionThread, 0) == WAIT_OBJECT_0) {
                        CloseHandle(g_hSessionThread);
                        g_hSessionThread = NULL;
                    }
                }

                if (g_hSessionThread == NULL) {
                    if (!bStorageInit) {
                        if (ImGui::Button("Start Session (Initialize First)", ImVec2(230, 32))) {
                            GuiLog("Error: Storage not initialized. Click 'Initialize Storage Now' first.");
                        }
                    } else {
                        if (ImGui::Button("Start Session", ImVec2(140, 32))) {
                            if (g_selectedMode == 0 && strlen(g_inputPass) == 0) {
                                GuiLog("Error 020: Password required to create room");
                            } else if (g_selectedMode == 1 && (strlen(g_inputRoom) == 0 || strlen(g_inputPass) == 0)) {
                                GuiLog("Error 020: Room code and password required");
                            } else {
                                g_hSessionThread = CreateThread(NULL, 0, SessionWorker, NULL, 0, NULL);
                            }
                        }
                    }
                } else {
                    if (ImGui::Button("Stop Session", ImVec2(140, 32))) {
                        running = false;
                        GuiLog("Stopping session...");
                        WaitForSingleObject(g_hSessionThread, 2000);
                        CloseHandle(g_hSessionThread);
                        g_hSessionThread = NULL;
                        if (shared != NULL) {
                            shared->status = SYNC_STATUS_DISCONNECTED;
                            shared->lobbyUsers = 0;
                            shared->lobbyMaxUsers = 0;
                            shared->room_code[0] = '\0';
                            snprintf(shared->status_msg, sizeof(shared->status_msg), "Session stopped");
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Live Session Status:");

                if (shared == NULL) {
                    initIPC();
                }

                if (shared != NULL) {
                    if (g_hSessionThread != NULL) {
                        ImGui::Text("Session: ");
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[ ACTIVE ]");
                        if (shared->room_code[0] != '\0') {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "| Room: %s (%s)", shared->room_code, shared->isHost ? "Host" : "Client");
                        }
                    } else {
                        ImGui::Text("Session: ");
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[ INACTIVE ]");
                    }

                    if (shared->lobbyMaxUsers > 0 && g_hSessionThread != NULL) {
                        ImGui::Text("Lobby Members: ");
                        ImGui::SameLine();
                        if (shared->lobbyUsers >= 2) {
                            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "%u / %u Connected (Ready)", shared->lobbyUsers, shared->lobbyMaxUsers);
                        } else {
                            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%u / %u (Waiting for partner...)", shared->lobbyUsers, shared->lobbyMaxUsers);
                        }
                    }
                    ImGui::Text("Current Tour: %u", shared->tour);
                    ImGui::SameLine();
                    ImGui::Text("| Status: ");
                    ImGui::SameLine();
                    switch (shared->status) {
                        case SYNC_STATUS_OK:
                            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "[ SYNCHRONIZED ]");
                            break;
                        case SYNC_STATUS_DESYNC:
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[ DESYNC DETECTED ]");
                            break;
                        case SYNC_STATUS_RESYNCING:
                            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[ RESYNCING ]");
                            break;
                        case SYNC_STATUS_CONNECTING:
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ CONNECTING ]");
                            break;
                        case SYNC_STATUS_DISCONNECTED:
                            ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "[ DISCONNECTED ]");
                            break;
                        default:
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[ IDLE ]");
                            break;
                    }

                    if (shared->status_msg[0] != '\0') {
                        if (shared->status == SYNC_STATUS_DISCONNECTED && (strstr(shared->status_msg, "Room does not exist") != NULL || strstr(shared->status_msg, "Pokój nie istnieje") != NULL)) {
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Błąd: Pokój nie istnieje / Room does not exist");
                        } else if (shared->status == SYNC_STATUS_DISCONNECTED && (strstr(shared->status_msg, "Invalid") != NULL || strstr(shared->status_msg, "full") != NULL || strstr(shared->status_msg, "already exists") != NULL || strstr(shared->status_msg, "error") != NULL || strstr(shared->status_msg, "Error") != NULL)) {
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Błąd: %s", shared->status_msg);
                        } else {
                            ImGui::Text("Message: %s", shared->status_msg);
                        }
                    }
                    if (shared->reloadCount > 0) {
                        ImGui::Text("Save Sync Dispatches: %u", shared->reloadCount);
                    }
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Błąd: IPC shared memory unavailable");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Tools")) {
                ImGui::Spacing();
                if (ImGui::Button("Initialize Storage (init)", ImVec2(220, 28))) {
                    if (init() == 0) {
                        GuiLog("Success: Storage initialized (.sync created)");
                    } else {
                        GuiLog("Error 007: Initialization failed");
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Create Snapshot (commit)", ImVec2(220, 28))) {
                    if (commit((char*)"Manual GUI commit") == 0) {
                        GuiLog("Success: Save snapshot commit created");
                    } else {
                        GuiLog("Error 018: Commit creation failed");
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Inject In-Game Overlay", ImVec2(220, 28))) {
                    DWORD pid = findShogunProcess();
                    if (pid == 0) {
                        GuiLog("Error 023: Shogun 2 process not found");
                    } else {
                        if (autoInjectShogun("shogun_overlay32.dll") == 0) {
                            GuiLog("Success: Overlay injected into Shogun 2");
                        } else {
                            GuiLog("Error 027: Remote overlay injection failed");
                        }
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("In-Game Hot-Reload (F9)", ImVec2(220, 28))) {
                    if (shared == NULL) {
                        initIPC();
                    }
                    if (shared != NULL) {
                        if (shared->room_code[0] != '\0') {
                            shared->reqSync = true;
                        } else {
                            shared->triggerReload = true;
                        }
                        strncpy(shared->lastSaveName, "autosave.save_multiplayer", sizeof(shared->lastSaveName) - 1);
                        shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
                        snprintf(shared->status_msg, sizeof(shared->status_msg), "In-game multiplayer reload triggered.");
                        GuiLog("In-game multiplayer reload triggered.");
                    } else {
                        GuiLog("Error 022: IPC shared memory not available");
                    }
                }

                DWORD shogunPid = findShogunProcess();
                ImGui::Spacing();
                if (shogunPid != 0) {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Shogun2.exe detected (PID: %lu)", shogunPid);
                } else {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Shogun2.exe not running");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Maintenance")) {
                ImGui::Spacing();
                ImGui::Text("Application Management:");

                if (ImGui::Button("Check & Install Update", ImVec2(220, 28))) {
                    GuiLog("Checking for updates...");
                    if (updateApplication() == 0) {
                        GuiLog("Update completed successfully. Please restart the app.");
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Uninstall Shogun-Sync", ImVec2(220, 28))) {
                    GuiLog("Uninstalling Shogun-Sync...");
                    uninstallApplication();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Author: KW");
                ImGui::Text("License: GNU GPL v2.0");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Console Activity Log:");
        ImGui::BeginChild("LogRegion", ImVec2(0, 110), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(g_guiLog);
        ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::End();

        ImGui::Render();
        const float clear_color[4] = { 0.10f, 0.10f, 0.12f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (shared != NULL) {
            shared->lastHeartbeatTick = GetTickCount();
        }

        g_pSwapChain->Present(1, 0);
    }

    if (shared != NULL) {
        shared->shouldExit = true;
    }

    if (g_hSessionThread != NULL) {
        running = false;
        WaitForSingleObject(g_hSessionThread, 1000);
        CloseHandle(g_hSessionThread);
        g_hSessionThread = NULL;
    }

    closeIPC();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    return 0;
}
