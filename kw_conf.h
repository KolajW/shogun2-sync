
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "kw_compress.h"
#include "kw_conn.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kw_shogun.h"
#include "kw_inject.h"

#if defined(_WIN32)
#include <direct.h>
#include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#endif

extern char *G_APPDATA;
extern char *G_LOCALAPPDATA;
extern volatile bool running;
extern bool connection;

extern int tour;
extern char latest[41];
extern char latestTree[41];
extern char latestActiveHash[41];
extern char g_previousSaveHash[41];
extern DWORD g_lastRestoreTick;
extern volatile bool g_isRestoringSave;
extern char hostname[256];
extern char msg[256];
extern bool isHost;

extern ShogunSharedData *shared;

typedef struct {
    char* type;
    char* hash;
    char* name;
} mapTree;

void test(char saveName[]);
void generateRoomCode(char *code, size_t len);
bool isStorageInitialized(void);
bool init();
bool getVariables();
bool scan(mapTree **mt, int *x);
bool getActiveCampaignSave(char *saveNameOut, size_t saveNameSize, char hashOut[41]);
bool add(char hashTree[41]);
bool commit(char comment[]);
bool watch();

bool initIPC();
void closeIPC();

char* getLatest();

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);