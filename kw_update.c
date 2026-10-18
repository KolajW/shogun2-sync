#include "kw_update.h"
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

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

static bool downloadFile(const char *url, const char *destPath){
    HINTERNET hInternet = InternetOpenA("ShogunSyncUpdater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        return false;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE;
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL)) {
        if (statusCode != 200) {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return false;
        }
    }

    FILE *f = fopen(destPath, "wb");
    if (!f) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    char buffer[8192];
    DWORD bytesRead = 0;
    bool success = true;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (fwrite(buffer, 1, bytesRead, f) != bytesRead) {
            success = false;
            break;
        }
    }

    fclose(f);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (!success) {
        DeleteFileA(destPath);
    }

    return success;
}

bool updateApplication(){
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
        printf("Error 030: Update check failed\n");
        return 1;
    }

    char dir[MAX_PATH];
    strncpy(dir, exePath, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *lastSlash = strrchr(dir, '\\');
    if (lastSlash != NULL) {
        *(lastSlash + 1) = '\0';
    }

    char newExe[MAX_PATH];
    char oldExe[MAX_PATH];
    snprintf(newExe, sizeof(newExe), "%sshogun-sync.new.exe", dir);
    snprintf(oldExe, sizeof(oldExe), "%sshogun-sync.old_%lu.exe", dir, GetTickCount());

    printf("Downloading latest version...\n");
    if (!downloadFile("https://shogun2-sync.com/downloads/shogun-sync.exe", newExe)) {
        if (!downloadFile("https://shogun2-sync.duckdns.org/download/shogun-sync.exe", newExe)) {
            if (!downloadFile("https://raw.githubusercontent.com/KolajW/shogun2-sync/main/shogun-sync.exe", newExe)) {
                printf("Error 031: Update download failed\n");
                return 1;
            }
        }
    }

    DeleteFileA(oldExe);
    if (!MoveFileA(exePath, oldExe)) {
        DeleteFileA(newExe);
        printf("Error 031: Update download failed\n");
        return 1;
    }

    if (!MoveFileA(newExe, exePath)) {
        MoveFileA(oldExe, exePath);
        printf("Error 031: Update download failed\n");
        return 1;
    }
    MoveFileExA(oldExe, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    char newGui[MAX_PATH];
    char oldGui[MAX_PATH];
    char guiPath[MAX_PATH];
    snprintf(guiPath, sizeof(guiPath), "%sShogunSync.exe", dir);
    snprintf(newGui, sizeof(newGui), "%sShogunSync.new.exe", dir);
    snprintf(oldGui, sizeof(oldGui), "%sShogunSync.old_%lu.exe", dir, GetTickCount());

    if (downloadFile("https://shogun2-sync.com/downloads/ShogunSync.exe", newGui) ||
        downloadFile("https://shogun2-sync.duckdns.org/download/ShogunSync.exe", newGui) ||
        downloadFile("https://raw.githubusercontent.com/KolajW/shogun2-sync/main/ShogunSync.exe", newGui)) {
        DeleteFileA(oldGui);
        if (MoveFileA(guiPath, oldGui)) {
            MoveFileExA(oldGui, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            MoveFileA(newGui, guiPath);
        } else {
            DeleteFileA(newGui);
        }
    }

    char newDll[MAX_PATH];
    char oldDll[MAX_PATH];
    char dllPath[MAX_PATH];
    snprintf(dllPath, sizeof(dllPath), "%sshogun_overlay32.dll", dir);
    snprintf(newDll, sizeof(newDll), "%sshogun_overlay32.new.dll", dir);
    snprintf(oldDll, sizeof(oldDll), "%sshogun_overlay32.old_%lu.dll", dir, GetTickCount());

    if (downloadFile("https://shogun2-sync.com/downloads/shogun_overlay32.dll", newDll) ||
        downloadFile("https://shogun2-sync.duckdns.org/download/shogun_overlay32.dll", newDll) ||
        downloadFile("https://raw.githubusercontent.com/KolajW/shogun2-sync/main/shogun_overlay32.dll", newDll)) {
        DeleteFileA(oldDll);
        MoveFileA(dllPath, oldDll);
        MoveFileExA(oldDll, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileA(newDll, dllPath);
    }

    printf("Update completed successfully\n");
    return 0;
}

static void cleanupDirectory(const char *dirPath, const char *currentExe) {
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", dirPath);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char itemPath[MAX_PATH];
        snprintf(itemPath, sizeof(itemPath), "%s\\%s", dirPath, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            cleanupDirectory(itemPath, currentExe);
            RemoveDirectoryA(itemPath);
        } else {
            SetFileAttributesA(itemPath, FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFileA(itemPath)) {
                char tempPath[MAX_PATH];
                GetTempPathA(sizeof(tempPath), tempPath);
                char tempFile[MAX_PATH];
                snprintf(tempFile, sizeof(tempFile), "%s%s_%lu.tmp", tempPath, fd.cFileName, GetTickCount());
                if (MoveFileA(itemPath, tempFile)) {
                    MoveFileExA(tempFile, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    RemoveDirectoryA(dirPath);
}

bool uninstallApplication(){
    killRunningProcesses();
    Sleep(200);

    char *localAppData = getenv("LOCALAPPDATA");
    if (localAppData == NULL) {
        printf("Error 032: Uninstall failed\n");
        return 1;
    }

    char targetDir[MAX_PATH];
    snprintf(targetDir, sizeof(targetDir), "%s\\ShogunSync", localAppData);

    char tempDir[MAX_PATH];
    GetTempPathA(sizeof(tempDir), tempDir);
    SetCurrentDirectoryA(tempDir);

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        char pathBuf[4096];
        DWORD pathLen = sizeof(pathBuf);
        DWORD type = REG_EXPAND_SZ;

        if (RegQueryValueExA(hKey, "PATH", NULL, &type, (LPBYTE)pathBuf, &pathLen) == ERROR_SUCCESS) {
            char newPath[4096] = {0};
            char *token = strtok(pathBuf, ";");
            while (token != NULL) {
                if (_stricmp(token, targetDir) != 0 && strstr(token, "ShogunSync") == NULL) {
                    if (newPath[0] != '\0') {
                        strncat(newPath, ";", sizeof(newPath) - strlen(newPath) - 1);
                    }
                    strncat(newPath, token, sizeof(newPath) - strlen(newPath) - 1);
                }
                token = strtok(NULL, ";");
            }

            RegSetValueExA(hKey, "PATH", 0, type, (const BYTE*)newPath, (DWORD)strlen(newPath) + 1);
        }
        RegCloseKey(hKey);

        DWORD_PTR result;
        SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, &result);
    }

    char desktopPath[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath) == S_OK) {
        char linkFile[MAX_PATH];
        snprintf(linkFile, sizeof(linkFile), "%s\\Shogun Sync.lnk", desktopPath);
        DeleteFileA(linkFile);
    }

    char startMenuPath[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath) == S_OK) {
        char linkFile[MAX_PATH];
        snprintf(linkFile, sizeof(linkFile), "%s\\Shogun Sync.lnk", startMenuPath);
        DeleteFileA(linkFile);
    }

    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ShogunSync");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{D37F8E3C-4321-4EBA-9D9A-2A478EFD6209}_is1");

    char currentExe[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, currentExe, sizeof(currentExe));

    cleanupDirectory(targetDir, currentExe);

    char cmdArgs[1024];
    snprintf(cmdArgs, sizeof(cmdArgs), "/c cd /d \"%s\" & for /l %%%%i in (1,1,20) do if exist \"%s\" (rmdir /s /q \"%s\" >nul 2>&1 & ping 127.0.0.1 -n 2 >nul)", tempDir, targetDir, targetDir);
    ShellExecuteA(NULL, "open", "cmd.exe", cmdArgs, tempDir, SW_HIDE);

    printf("Shogun-Sync uninstalled successfully\n");
    return 0;
}
