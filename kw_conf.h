#include "kw_compress.h"
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>

#if defined(_WIN32)
#include <direct.h>
#include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#endif

extern char *G_APPDATA;
extern char *G_LOCALAPPDATA;
extern volatile bool running;

typedef struct {
    char* type;
    char* hash;
    char* name;
} mapTree;

void test(char saveName[]);
bool init();
bool getVariables();
bool scan(mapTree **mt, int *x);
bool add(char hashTree[41]);
bool commit(char comment[]);
bool watch();
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);