#include "kw_update.h"
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

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
    snprintf(oldExe, sizeof(oldExe), "%sshogun-sync.old.exe", dir);

    printf("Downloading latest version...\n");
    if (!downloadFile("https://shogun2-sync.duckdns.org/download/shogun-sync.exe", newExe)) {
        if (!downloadFile("https://raw.githubusercontent.com/KolajW/shogun2-sync/main/shogun-sync.exe", newExe)) {
            printf("Error 031: Update download failed\n");
            return 1;
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

    char newDll[MAX_PATH];
    char oldDll[MAX_PATH];
    char dllPath[MAX_PATH];
    snprintf(dllPath, sizeof(dllPath), "%sshogun_overlay32.dll", dir);
    snprintf(newDll, sizeof(newDll), "%sshogun_overlay32.new.dll", dir);
    snprintf(oldDll, sizeof(oldDll), "%sshogun_overlay32.old.dll", dir);

    if (downloadFile("https://shogun2-sync.duckdns.org/download/shogun_overlay32.dll", newDll) ||
        downloadFile("https://raw.githubusercontent.com/KolajW/shogun2-sync/main/shogun_overlay32.dll", newDll)) {
        DeleteFileA(oldDll);
        MoveFileA(dllPath, oldDll);
        MoveFileA(newDll, dllPath);
    }

    printf("Update completed successfully\n");
    return 0;
}

bool uninstallApplication(){
    char *localAppData = getenv("LOCALAPPDATA");
    if (localAppData == NULL) {
        printf("Error 032: Uninstall failed\n");
        return 1;
    }

    char targetDir[MAX_PATH];
    snprintf(targetDir, sizeof(targetDir), "%s\\ShogunSync", localAppData);

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

    char cmdArgs[512];
    snprintf(cmdArgs, sizeof(cmdArgs), "/c timeout /t 2 /nobreak >nul & rmdir /s /q \"%s\"", targetDir);
    ShellExecuteA(NULL, "open", "cmd.exe", cmdArgs, NULL, SW_HIDE);

    printf("Shogun-Sync uninstalled successfully\n");
    return 0;
}
