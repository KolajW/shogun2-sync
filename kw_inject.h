#ifndef KW_INJECT_H
#define KW_INJECT_H

#include <windows.h>
#include <stdbool.h>

DWORD findShogunProcess();
bool isProcess64Bit(DWORD pid);
bool isDLLInjected(DWORD pid, const char *dllName);
bool injectDLL(DWORD pid, const char *dllPath);
bool autoInjectShogun(const char *preferredDll);

#endif
