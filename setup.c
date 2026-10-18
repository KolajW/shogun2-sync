#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdbool.h>

#define IDC_BTN_INSTALL 1001
#define IDC_BTN_CANCEL 1002
#define IDC_CHK_DESKTOP 1003
#define IDC_CHK_STARTMENU 1004
#define IDC_CHK_PATH 1005
#define IDC_CHK_UNINSTALL 1006
#define IDC_CHK_LAUNCH 1007
#define IDC_PROGRESS 1008
#define IDC_STATUS 1009
#define IDC_DIR_EDIT 1010
#define WM_INSTALL_COMPLETE (WM_USER + 100)
#define WM_INSTALL_PROGRESS (WM_USER + 101)

static HWND g_hInstallBtn = NULL;
static HWND g_hCancelBtn = NULL;
static HWND g_hChkDesktop = NULL;
static HWND g_hChkStartMenu = NULL;
static HWND g_hChkPath = NULL;
static HWND g_hChkUninstall = NULL;
static HWND g_hChkLaunch = NULL;
static HWND g_hProgress = NULL;
static HWND g_hStatus = NULL;
static HWND g_hDirEdit = NULL;

static bool g_installed = false;
static bool g_launched = false;
static char g_targetDir[MAX_PATH] = "";

static bool extractResource(int resId, const char *destPath) {
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(NULL, hRes);
    void *ptr = LockResource(hData);
    if (!ptr || size == 0) return false;

    FILE *f = fopen(destPath, "wb");
    if (!f) {
        char tempOld[MAX_PATH];
        snprintf(tempOld, sizeof(tempOld), "%s.old_%lu.tmp", destPath, GetTickCount());
        DeleteFileA(tempOld);
        if (MoveFileA(destPath, tempOld)) {
            MoveFileExA(tempOld, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            f = fopen(destPath, "wb");
        }
    }
    if (!f) return false;
    size_t written = fwrite(ptr, 1, size, f);
    fclose(f);
    return (written == size);
}

static bool copyOrExtract(int resId, const char *fileName, const char *destPath) {
    if (extractResource(resId, destPath)) {
        return true;
    }
    char localPath[MAX_PATH];
    char exeDir[MAX_PATH];
    if (GetModuleFileNameA(NULL, exeDir, MAX_PATH) != 0) {
        char *lastSlash = strrchr(exeDir, '\\');
        if (lastSlash != NULL) {
            *(lastSlash + 1) = '\0';
            snprintf(localPath, sizeof(localPath), "%s%s", exeDir, fileName);
            if (CopyFileA(localPath, destPath, FALSE)) {
                return true;
            }
            char tempOld[MAX_PATH];
            snprintf(tempOld, sizeof(tempOld), "%s.old_%lu.tmp", destPath, GetTickCount());
            DeleteFileA(tempOld);
            if (MoveFileA(destPath, tempOld)) {
                MoveFileExA(tempOld, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                if (CopyFileA(localPath, destPath, FALSE)) {
                    return true;
                }
            }
        }
    }
    return false;
}

static bool createShortcut(const char *targetPath, const char *shortcutPath, const char *description, const char *workingDir) {
    CoInitialize(NULL);
    IShellLinkA *pShellLink = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkA, (void**)&pShellLink);
    if (SUCCEEDED(hr)) {
        pShellLink->lpVtbl->SetPath(pShellLink, targetPath);
        if (description != NULL) {
            pShellLink->lpVtbl->SetDescription(pShellLink, description);
        }
        if (workingDir != NULL) {
            pShellLink->lpVtbl->SetWorkingDirectory(pShellLink, workingDir);
        }
        IPersistFile *pPersistFile = NULL;
        hr = pShellLink->lpVtbl->QueryInterface(pShellLink, &IID_IPersistFile, (void**)&pPersistFile);
        if (SUCCEEDED(hr)) {
            wchar_t wShortcutPath[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, shortcutPath, -1, wShortcutPath, MAX_PATH);
            hr = pPersistFile->lpVtbl->Save(pPersistFile, wShortcutPath, TRUE);
            pPersistFile->lpVtbl->Release(pPersistFile);
        }
        pShellLink->lpVtbl->Release(pShellLink);
    }
    CoUninitialize();
    return SUCCEEDED(hr);
}

static void killRunningProcesses(void) {
    DWORD currentPid = GetCurrentProcessId();
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID != currentPid) {
                    if (_stricmp(pe.szExeFile, "shogun-sync.exe") == 0 ||
                        _stricmp(pe.szExeFile, "ShogunSync.exe") == 0) {
                        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hProc != NULL) {
                            TerminateProcess(hProc, 0);
                            CloseHandle(hProc);
                        }
                    }
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "taskkill /f /fi \"PID ne %lu\" /im shogun-sync.exe /im ShogunSync.exe >nul 2>&1", currentPid);
    system(cmd);
}

static bool addPathEnvironment(const char *dir) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    char pathBuf[4096] = {0};
    DWORD pathLen = sizeof(pathBuf);
    DWORD type = REG_EXPAND_SZ;

    RegQueryValueExA(hKey, "PATH", NULL, &type, (LPBYTE)pathBuf, &pathLen);

    if (strstr(pathBuf, dir) == NULL) {
        char newPath[4096];
        if (strlen(pathBuf) > 0) {
            snprintf(newPath, sizeof(newPath), "%s;%s", pathBuf, dir);
        } else {
            snprintf(newPath, sizeof(newPath), "%s", dir);
        }
        RegSetValueExA(hKey, "PATH", 0, type, (const BYTE*)newPath, (DWORD)strlen(newPath) + 1);
    }

    RegCloseKey(hKey);

    SendNotifyMessageA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment");
    return true;
}

static bool registerUninstall(const char *targetDir) {
    HKEY hKey;
    const char *subKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ShogunSync";
    if (RegCreateKeyExA(HKEY_CURRENT_USER, subKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }

    const char *name = "Shogun Sync";
    const char *ver = "0.1.3-beta";
    const char *publisher = "KW";

    char uninstallCmd[MAX_PATH + 32];
    snprintf(uninstallCmd, sizeof(uninstallCmd), "\"%s\\shogun-sync.exe\" uninstall", targetDir);

    char displayIcon[MAX_PATH + 16];
    snprintf(displayIcon, sizeof(displayIcon), "\"%s\\ShogunSync.exe\",0", targetDir);

    DWORD dwordOne = 1;

    RegSetValueExA(hKey, "DisplayName", 0, REG_SZ, (const BYTE*)name, (DWORD)strlen(name) + 1);
    RegSetValueExA(hKey, "DisplayVersion", 0, REG_SZ, (const BYTE*)ver, (DWORD)strlen(ver) + 1);
    RegSetValueExA(hKey, "Publisher", 0, REG_SZ, (const BYTE*)publisher, (DWORD)strlen(publisher) + 1);
    RegSetValueExA(hKey, "InstallLocation", 0, REG_SZ, (const BYTE*)targetDir, (DWORD)strlen(targetDir) + 1);
    RegSetValueExA(hKey, "UninstallString", 0, REG_SZ, (const BYTE*)uninstallCmd, (DWORD)strlen(uninstallCmd) + 1);
    RegSetValueExA(hKey, "DisplayIcon", 0, REG_SZ, (const BYTE*)displayIcon, (DWORD)strlen(displayIcon) + 1);
    RegSetValueExA(hKey, "NoModify", 0, REG_DWORD, (const BYTE*)&dwordOne, sizeof(DWORD));
    RegSetValueExA(hKey, "NoRepair", 0, REG_DWORD, (const BYTE*)&dwordOne, sizeof(DWORD));

    RegCloseKey(hKey);

    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{D37F8E3C-4321-4EBA-9D9A-2A478EFD6209}_is1");
    return true;
}

typedef struct {
    HWND hWnd;
    bool makeDesktop;
    bool makeStartMenu;
    bool makePath;
    bool makeUninstall;
} InstallParams;

static DWORD WINAPI InstallThreadProc(LPVOID lpParam) {
    InstallParams *params = (InstallParams*)lpParam;
    HWND hWnd = params->hWnd;
    bool d = params->makeDesktop;
    bool s = params->makeStartMenu;
    bool p = params->makePath;
    bool u = params->makeUninstall;
    free(params);

    killRunningProcesses();
    Sleep(100);

    CreateDirectoryA(g_targetDir, NULL);

    char saveDir[MAX_PATH];
    snprintf(saveDir, sizeof(saveDir), "%s\\saves", g_targetDir);
    CreateDirectoryA(saveDir, NULL);

    PostMessageA(hWnd, WM_INSTALL_PROGRESS, 20, 0);

    char exeTarget[MAX_PATH];
    char guiTarget[MAX_PATH];
    char dllTarget[MAX_PATH];

    snprintf(exeTarget, sizeof(exeTarget), "%s\\shogun-sync.exe", g_targetDir);
    snprintf(guiTarget, sizeof(guiTarget), "%s\\ShogunSync.exe", g_targetDir);
    snprintf(dllTarget, sizeof(dllTarget), "%s\\shogun_overlay32.dll", g_targetDir);

    if (!copyOrExtract(101, "shogun-sync.exe", exeTarget)) {
        PostMessageA(hWnd, WM_INSTALL_COMPLETE, 0, 0);
        return 1;
    }

    PostMessageA(hWnd, WM_INSTALL_PROGRESS, 45, 0);

    if (!copyOrExtract(102, "ShogunSync.exe", guiTarget)) {
        PostMessageA(hWnd, WM_INSTALL_COMPLETE, 0, 0);
        return 1;
    }

    PostMessageA(hWnd, WM_INSTALL_PROGRESS, 65, 0);

    if (!copyOrExtract(103, "shogun_overlay32.dll", dllTarget)) {
        PostMessageA(hWnd, WM_INSTALL_COMPLETE, 0, 0);
        return 1;
    }

    char pthreadTarget[MAX_PATH];
    snprintf(pthreadTarget, sizeof(pthreadTarget), "%s\\libwinpthread-1.dll", g_targetDir);
    copyOrExtract(104, "libwinpthread-1.dll", pthreadTarget);

    char cerTarget[MAX_PATH];
    snprintf(cerTarget, sizeof(cerTarget), "%s\\ShogunSync.cer", g_targetDir);
    if (copyOrExtract(105, "ShogunSync.cer", cerTarget)) {
        char certCmd[MAX_PATH + 80];
        snprintf(certCmd, sizeof(certCmd), "certutil -addstore -user -f \"TrustedPublisher\" \"%s\"", cerTarget);
        system(certCmd);
        snprintf(certCmd, sizeof(certCmd), "certutil -addstore -user \"Root\" \"%s\"", cerTarget);
        system(certCmd);
    }

    PostMessageA(hWnd, WM_INSTALL_PROGRESS, 80, 0);

    if (d) {
        char desktopPath[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath) == S_OK) {
            char shortcutFile[MAX_PATH];
            snprintf(shortcutFile, sizeof(shortcutFile), "%s\\Shogun Sync.lnk", desktopPath);
            createShortcut(guiTarget, shortcutFile, "Total War: Shogun 2 Synchronization Tool", g_targetDir);
        }
    }

    if (s) {
        char startMenuPath[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath) == S_OK) {
            char shortcutFile[MAX_PATH];
            snprintf(shortcutFile, sizeof(shortcutFile), "%s\\Shogun Sync.lnk", startMenuPath);
            createShortcut(guiTarget, shortcutFile, "Total War: Shogun 2 Synchronization Tool", g_targetDir);
        }
    }

    if (p) {
        addPathEnvironment(g_targetDir);
    }

    if (u) {
        registerUninstall(g_targetDir);
    }

    PostMessageA(hWnd, WM_INSTALL_PROGRESS, 100, 0);
    PostMessageA(hWnd, WM_INSTALL_COMPLETE, 1, 0);
    return 0;
}

static void launchTargetApp() {
    if (g_launched) return;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    char cmdLine[MAX_PATH + 32];
    snprintf(cmdLine, sizeof(cmdLine), "\"%s\\ShogunSync.exe\"", g_targetDir);
    if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, g_targetDir, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        g_launched = true;
    } else {
        char targetExe[MAX_PATH];
        snprintf(targetExe, sizeof(targetExe), "%s\\ShogunSync.exe", g_targetDir);
        ShellExecuteA(NULL, "open", targetExe, NULL, g_targetDir, SW_SHOWNORMAL);
        g_launched = true;
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hFontBold = CreateFontA(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HFONT hFontSub = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HFONT hFontNormal = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            HWND hTitle = CreateWindowExA(0, "STATIC", "Total War: Shogun 2 Synchronization Tool", WS_CHILD | WS_VISIBLE, 20, 14, 460, 22, hWnd, NULL, NULL, NULL);
            SendMessageA(hTitle, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hSub = CreateWindowExA(0, "STATIC", "Setup Wizard - v0.1.3-beta", WS_CHILD | WS_VISIBLE, 20, 38, 460, 18, hWnd, NULL, NULL, NULL);
            SendMessageA(hSub, WM_SETFONT, (WPARAM)hFontSub, TRUE);

            CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 20, 62, 460, 2, hWnd, NULL, NULL, NULL);

            HWND hGrpDir = CreateWindowExA(0, "BUTTON", "Destination Folder", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 72, 460, 64, hWnd, NULL, NULL, NULL);
            SendMessageA(hGrpDir, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            g_hDirEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_targetDir, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 36, 96, 428, 24, hWnd, (HMENU)IDC_DIR_EDIT, NULL, NULL);
            SendMessageA(g_hDirEdit, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            HWND hGrpOpt = CreateWindowExA(0, "BUTTON", "Installation Options", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 144, 460, 120, hWnd, NULL, NULL, NULL);
            SendMessageA(hGrpOpt, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            g_hChkDesktop = CreateWindowExA(0, "BUTTON", "Create Desktop shortcut (Shogun Sync)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 36, 166, 380, 20, hWnd, (HMENU)IDC_CHK_DESKTOP, NULL, NULL);
            SendMessageA(g_hChkDesktop, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessageA(g_hChkDesktop, BM_SETCHECK, BST_CHECKED, 0);

            g_hChkStartMenu = CreateWindowExA(0, "BUTTON", "Create Start Menu shortcut (Quick Access)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 36, 188, 380, 20, hWnd, (HMENU)IDC_CHK_STARTMENU, NULL, NULL);
            SendMessageA(g_hChkStartMenu, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessageA(g_hChkStartMenu, BM_SETCHECK, BST_CHECKED, 0);

            g_hChkPath = CreateWindowExA(0, "BUTTON", "Add to user PATH environment variable (CLI access)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 36, 210, 380, 20, hWnd, (HMENU)IDC_CHK_PATH, NULL, NULL);
            SendMessageA(g_hChkPath, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessageA(g_hChkPath, BM_SETCHECK, BST_CHECKED, 0);

            g_hChkUninstall = CreateWindowExA(0, "BUTTON", "Register in Windows Add/Remove Programs", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 36, 232, 380, 20, hWnd, (HMENU)IDC_CHK_UNINSTALL, NULL, NULL);
            SendMessageA(g_hChkUninstall, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessageA(g_hChkUninstall, BM_SETCHECK, BST_CHECKED, 0);

            g_hProgress = CreateWindowExA(0, PROGRESS_CLASSA, NULL, WS_CHILD | WS_VISIBLE, 20, 274, 460, 18, hWnd, (HMENU)IDC_PROGRESS, NULL, NULL);
            SendMessageA(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessageA(g_hProgress, PBM_SETPOS, 0, 0);

            g_hStatus = CreateWindowExA(0, "STATIC", "Click Install to begin setup.", WS_CHILD | WS_VISIBLE, 20, 298, 460, 18, hWnd, (HMENU)IDC_STATUS, NULL, NULL);
            SendMessageA(g_hStatus, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            g_hChkLaunch = CreateWindowExA(0, "BUTTON", "Launch Shogun Sync after installation", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 320, 300, 20, hWnd, (HMENU)IDC_CHK_LAUNCH, NULL, NULL);
            SendMessageA(g_hChkLaunch, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessageA(g_hChkLaunch, BM_SETCHECK, BST_CHECKED, 0);

            HWND hAuthor = CreateWindowExA(0, "STATIC", "Author: KW | GNU GPL v2.0", WS_CHILD | WS_VISIBLE, 20, 350, 220, 18, hWnd, NULL, NULL, NULL);
            SendMessageA(hAuthor, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            g_hInstallBtn = CreateWindowExA(0, "BUTTON", "Install", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 300, 344, 95, 28, hWnd, (HMENU)IDC_BTN_INSTALL, NULL, NULL);
            SendMessageA(g_hInstallBtn, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            g_hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 405, 344, 75, 28, hWnd, (HMENU)IDC_BTN_CANCEL, NULL, NULL);
            SendMessageA(g_hCancelBtn, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_BTN_INSTALL) {
                if (g_installed) {
                    if (SendMessageA(g_hChkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        launchTargetApp();
                    }
                    DestroyWindow(hWnd);
                    return 0;
                }

                EnableWindow(g_hInstallBtn, FALSE);
                EnableWindow(g_hCancelBtn, FALSE);
                EnableWindow(g_hChkDesktop, FALSE);
                EnableWindow(g_hChkStartMenu, FALSE);
                EnableWindow(g_hChkPath, FALSE);
                EnableWindow(g_hChkUninstall, FALSE);

                SetWindowTextA(g_hStatus, "Installing Shogun Sync files...");
                SendMessageA(g_hProgress, PBM_SETPOS, 10, 0);

                InstallParams *params = (InstallParams*)malloc(sizeof(InstallParams));
                params->hWnd = hWnd;
                params->makeDesktop = (SendMessageA(g_hChkDesktop, BM_GETCHECK, 0, 0) == BST_CHECKED);
                params->makeStartMenu = (SendMessageA(g_hChkStartMenu, BM_GETCHECK, 0, 0) == BST_CHECKED);
                params->makePath = (SendMessageA(g_hChkPath, BM_GETCHECK, 0, 0) == BST_CHECKED);
                params->makeUninstall = (SendMessageA(g_hChkUninstall, BM_GETCHECK, 0, 0) == BST_CHECKED);

                HANDLE hThread = CreateThread(NULL, 0, InstallThreadProc, params, 0, NULL);
                if (hThread != NULL) {
                    CloseHandle(hThread);
                } else {
                    free(params);
                    SetWindowTextA(g_hStatus, "Error 034: Installation failed");
                    EnableWindow(g_hInstallBtn, TRUE);
                    EnableWindow(g_hCancelBtn, TRUE);
                }
            } else if (wmId == IDC_BTN_CANCEL) {
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_INSTALL_PROGRESS: {
            SendMessageA(g_hProgress, PBM_SETPOS, wParam, 0);
            break;
        }

        case WM_INSTALL_COMPLETE: {
            if (wParam == 1) {
                SendMessageA(g_hProgress, PBM_SETPOS, 100, 0);
                SetWindowTextA(g_hStatus, "Installation completed successfully.");
                SetWindowTextA(g_hInstallBtn, "Finish");
                EnableWindow(g_hInstallBtn, TRUE);
                g_installed = true;
                if (SendMessageA(g_hChkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    launchTargetApp();
                }
            } else {
                SetWindowTextA(g_hStatus, "Error 034: Installation failed");
                EnableWindow(g_hInstallBtn, TRUE);
                EnableWindow(g_hCancelBtn, TRUE);
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    char *localAppData = getenv("LOCALAPPDATA");
    if (localAppData == NULL) {
        return 1;
    }
    snprintf(g_targetDir, sizeof(g_targetDir), "%s\\ShogunSync", localAppData);

    if (strstr(lpCmdLine, "/S") != NULL || strstr(lpCmdLine, "--silent") != NULL || strstr(lpCmdLine, "-s") != NULL) {
        killRunningProcesses();
        CreateDirectoryA(g_targetDir, NULL);
        char saveDir[MAX_PATH];
        snprintf(saveDir, sizeof(saveDir), "%s\\saves", g_targetDir);
        CreateDirectoryA(saveDir, NULL);
        char exeTarget[MAX_PATH];
        char guiTarget[MAX_PATH];
        char dllTarget[MAX_PATH];
        snprintf(exeTarget, sizeof(exeTarget), "%s\\shogun-sync.exe", g_targetDir);
        snprintf(guiTarget, sizeof(guiTarget), "%s\\ShogunSync.exe", g_targetDir);
        snprintf(dllTarget, sizeof(dllTarget), "%s\\shogun_overlay32.dll", g_targetDir);
        bool res = copyOrExtract(101, "shogun-sync.exe", exeTarget) &&
                   copyOrExtract(102, "ShogunSync.exe", guiTarget) &&
                   copyOrExtract(103, "shogun_overlay32.dll", dllTarget);
        char cerTarget[MAX_PATH];
        snprintf(cerTarget, sizeof(cerTarget), "%s\\ShogunSync.cer", g_targetDir);
        copyOrExtract(105, "ShogunSync.cer", cerTarget);
        if (res) {
            char desktopPath[MAX_PATH];
            if (SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath) == S_OK) {
                char shortcutFile[MAX_PATH];
                snprintf(shortcutFile, sizeof(shortcutFile), "%s\\Shogun Sync.lnk", desktopPath);
                createShortcut(guiTarget, shortcutFile, "Total War: Shogun 2 Synchronization Tool", g_targetDir);
            }
            char startMenuPath[MAX_PATH];
            if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath) == S_OK) {
                char shortcutFile[MAX_PATH];
                snprintf(shortcutFile, sizeof(shortcutFile), "%s\\Shogun Sync.lnk", startMenuPath);
                createShortcut(guiTarget, shortcutFile, "Total War: Shogun 2 Synchronization Tool", g_targetDir);
            }
            addPathEnvironment(g_targetDir);
            registerUninstall(g_targetDir);
        }
        return res ? 0 : 1;
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "ShogunSyncSetupWindow";

    RegisterClassExA(&wc);

    int w = 516;
    int h = 425;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hWnd = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        wc.lpszClassName,
        "Shogun Sync Setup - v0.1.3-beta",
        WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
