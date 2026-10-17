#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HWND hExisting = FindWindowA("ShogunSyncGUI", "Shogun Sync");
    if (hExisting != NULL) {
        ShowWindow(hExisting, SW_RESTORE);
        SetForegroundWindow(hExisting);
        return 0;
    }

    char exeDir[MAX_PATH];
    if (GetModuleFileNameA(NULL, exeDir, MAX_PATH) == 0) {
        return 1;
    }

    char *lastSlash = strrchr(exeDir, '\\');
    if (lastSlash != NULL) {
        *(lastSlash + 1) = '\0';
    }

    char exePath[MAX_PATH];
    snprintf(exePath, sizeof(exePath), "%sshogun-sync.exe", exeDir);

    char cmdLine[MAX_PATH + 16];
    snprintf(cmdLine, sizeof(cmdLine), "\"%s\" --gui", exePath);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, exeDir, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }

    return 1;
}
