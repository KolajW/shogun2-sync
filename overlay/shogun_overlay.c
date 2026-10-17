#include <windows.h>
#include <stdio.h>
#include "../kw_shogun.h"

static HANDLE hIPC = NULL;
static ShogunSharedData *pShared = NULL;
static HANDLE hThread = NULL;
static volatile BOOL bRunning = TRUE;

DWORD WINAPI OverlayThread(LPVOID lpParam){
    hIPC = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\ShogunSyncIPC");
    if (hIPC != NULL) {
        pShared = (ShogunSharedData *)MapViewOfFile(hIPC, FILE_MAP_READ, 0, 0, sizeof(ShogunSharedData));
    }

    char logPath[MAX_PATH];
    char *localAppData = getenv("LOCALAPPDATA");
    if (localAppData != NULL) {
        snprintf(logPath, sizeof(logPath), "%s\\ShogunSync\\overlay.log", localAppData);
    } else {
        snprintf(logPath, sizeof(logPath), "shogun_overlay.log");
    }

    FILE *f = fopen(logPath, "a");
    if (f != NULL) {
        fprintf(f, "Shogun-Sync overlay injected successfully into PID %lu\n", GetCurrentProcessId());
        if (pShared != NULL) {
            fprintf(f, "IPC connected: Tour %u, Status %u, Msg: %s\n", pShared->tour, pShared->status, pShared->status_msg);
        } else {
            fprintf(f, "IPC mapping not found\n");
        }
        fclose(f);
    }

    Beep(1000, 200);

    while (bRunning) {
        Sleep(100);
    }

    if (pShared != NULL) {
        UnmapViewOfFile(pShared);
        pShared = NULL;
    }
    if (hIPC != NULL) {
        CloseHandle(hIPC);
        hIPC = NULL;
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved){
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            hThread = CreateThread(NULL, 0, OverlayThread, NULL, 0, NULL);
            break;
        case DLL_PROCESS_DETACH:
            bRunning = FALSE;
            if (hThread != NULL) {
                WaitForSingleObject(hThread, 1000);
                CloseHandle(hThread);
            }
            break;
    }
    return TRUE;
}
