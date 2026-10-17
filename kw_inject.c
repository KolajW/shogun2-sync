#include "kw_inject.h"
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

DWORD findShogunProcess(){
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, "Shogun2.exe") == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32Next(snap, &pe));
    }

    CloseHandle(snap);
    return 0;
}

bool isProcess64Bit(DWORD pid){
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (proc == NULL) {
        return false;
    }

    BOOL isWow64 = FALSE;
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        CloseHandle(proc);
        return false;
    }

    if (IsWow64Process(proc, &isWow64)) {
        CloseHandle(proc);
        return isWow64 == FALSE;
    }

    CloseHandle(proc);
    return false;
}

static void enableDebugPrivilege(void){
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp;
        LUID luid;
        if (LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        }
        CloseHandle(hToken);
    }
}

bool isDLLInjected(DWORD pid, const char *dllName){
    enableDebugPrivilege();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32 me;
    me.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snap, &me)) {
        do {
            if (_stricmp(me.szModule, dllName) == 0) {
                CloseHandle(snap);
                return true;
            }
        } while (Module32Next(snap, &me));
    }

    CloseHandle(snap);
    return false;
}

static bool resolveDLLPath(const char *dllName, char *outFullPath, size_t outSize){
    if (GetFullPathNameA(dllName, outSize, outFullPath, NULL) != 0) {
        DWORD attrs = GetFileAttributesA(outFullPath);
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return true;
        }
    }

    char exeDir[MAX_PATH];
    if (GetModuleFileNameA(NULL, exeDir, sizeof(exeDir)) != 0) {
        char *lastSlash = strrchr(exeDir, '\\');
        if (lastSlash != NULL) {
            *(lastSlash + 1) = '\0';
            snprintf(outFullPath, outSize, "%s%s", exeDir, dllName);
            DWORD attrs = GetFileAttributesA(outFullPath);
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return true;
            }
        }
    }

    char *localAppData = getenv("LOCALAPPDATA");
    if (localAppData != NULL) {
        snprintf(outFullPath, outSize, "%s\\ShogunSync\\%s", localAppData, dllName);
        DWORD attrs = GetFileAttributesA(outFullPath);
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return true;
        }
    }

    return false;
}

bool injectDLL(DWORD pid, const char *dllPath){
    enableDebugPrivilege();
    char fullPath[MAX_PATH];
    if (!resolveDLLPath(dllPath, fullPath, sizeof(fullPath))) {
        printf("Error 028: Overlay file not found: %s\n", dllPath);
        return 1;
    }

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (proc == NULL) {
        printf("Error 024: Process open failed\n");
        return 1;
    }

    size_t pathLen = strlen(fullPath) + 1;
    LPVOID remoteBuf = VirtualAllocEx(proc, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remoteBuf == NULL) {
        printf("Error 025: Remote memory allocation failed\n");
        CloseHandle(proc);
        return 1;
    }

    if (!WriteProcessMemory(proc, remoteBuf, fullPath, pathLen, NULL)) {
        printf("Error 026: Remote memory write failed\n");
        VirtualFreeEx(proc, remoteBuf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }

    FARPROC loadLib = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (loadLib == NULL) {
        printf("Error 027: Remote thread creation failed\n");
        VirtualFreeEx(proc, remoteBuf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }

    HANDLE thread = CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, remoteBuf, 0, NULL);
    if (thread == NULL) {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll != NULL) {
            typedef LONG (NTAPI *pfnNtCreateThreadEx)(
                PHANDLE ThreadHandle,
                ACCESS_MASK DesiredAccess,
                LPVOID ObjectAttributes,
                HANDLE ProcessHandle,
                LPVOID lpStartAddress,
                LPVOID lpParameter,
                ULONG CreateSuspended,
                SIZE_T ZeroBits,
                SIZE_T SizeOfStackCommit,
                SIZE_T SizeOfStackReserve,
                LPVOID lpBytesBuffer
            );
            pfnNtCreateThreadEx pNtCreateThreadEx = (pfnNtCreateThreadEx)GetProcAddress(hNtdll, "NtCreateThreadEx");
            if (pNtCreateThreadEx != NULL) {
                pNtCreateThreadEx(&thread, 0x1FFFFF, NULL, proc, (LPVOID)loadLib, remoteBuf, 0, 0, 0, 0, NULL);
            }
        }
    }

    if (thread == NULL) {
        printf("Error 027: Remote thread creation failed\n");
        VirtualFreeEx(proc, remoteBuf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }

    WaitForSingleObject(thread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);

    CloseHandle(thread);
    VirtualFreeEx(proc, remoteBuf, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (exitCode == 0) {
        printf("Error 029: Remote library load failed\n");
        return 1;
    }

    printf("Overlay injected successfully\n");
    return 0;
}

bool autoInjectShogun(const char *preferredDll){
    DWORD pid = findShogunProcess();
    if (pid == 0) {
        return 1;
    }

    char dllToUse[MAX_PATH];
    if (preferredDll != NULL && strlen(preferredDll) > 0) {
        strncpy(dllToUse, preferredDll, sizeof(dllToUse) - 1);
        dllToUse[sizeof(dllToUse) - 1] = '\0';
    } else {
        bool is64 = isProcess64Bit(pid);
        if (is64) {
            snprintf(dllToUse, sizeof(dllToUse), "shogun_overlay64.dll");
        } else {
            snprintf(dllToUse, sizeof(dllToUse), "shogun_overlay32.dll");
        }
    }

    char filename[MAX_PATH];
    const char *lastSlash = strrchr(dllToUse, '\\');
    const char *lastFwdSlash = strrchr(dllToUse, '/');
    if (lastSlash != NULL) {
        strncpy(filename, lastSlash + 1, sizeof(filename) - 1);
    } else if (lastFwdSlash != NULL) {
        strncpy(filename, lastFwdSlash + 1, sizeof(filename) - 1);
    } else {
        strncpy(filename, dllToUse, sizeof(filename) - 1);
    }
    filename[sizeof(filename) - 1] = '\0';

    if (isDLLInjected(pid, filename)) {
        return 0;
    }

    char fullPath[MAX_PATH];
    if (!resolveDLLPath(dllToUse, fullPath, sizeof(fullPath))) {
        return 1;
    }

    return injectDLL(pid, fullPath);
}
