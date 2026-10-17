#include "kw_conf.h"

char *G_APPDATA;
char *G_LOCALAPPDATA;
volatile bool running = true;
bool connection = false;
bool isHost = false;
char hostname[256];

int tour = 0;
char latest[41];
char latestTree[41] = {0};
char latestActiveHash[41] = {0};
char g_previousSaveHash[41] = {0};
DWORD g_lastRestoreTick = 0;
volatile bool g_isRestoringSave = false;
char msg[256];

static HANDLE shogunHandle = NULL;
ShogunSharedData *shared = NULL;

bool getVariables(){
    G_APPDATA = getenv("APPDATA");
    if (G_APPDATA == NULL) {
        printf("Error 005: AppData path not found\n");
        return 1;
    }

    G_LOCALAPPDATA = getenv("LOCALAPPDATA");
    if (G_LOCALAPPDATA == NULL) {
        printf("Error 006: LocalAppData path not found\n");
        return 1;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Error 014: Hostname resolution failed\n");
        return 1;
    }
#endif
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
    }
    return 0;
}

void test(char saveName[]){
    char savePath[512];
    snprintf(savePath, sizeof(savePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s.save_multiplayer", G_APPDATA,saveName);
    
    FILE *save = fopen(savePath,"rb");
    if (save == NULL) {
        printf("Error 001: Save file not found\n");
        return;
    }

    fseek(save, 0L, SEEK_END);
    size_t ML = ftell(save);
    rewind(save);

    uint8_t *buff = malloc(ML);
    if (buff == NULL) {
        fclose(save);
        printf("Error 017: Memory allocation failed\n");
        return;
    }
    fread(buff,1,ML,save);
    uint32_t result[5];
    sha1(buff,ML,result);

    fclose(save);

    printS(result);
    char filename[41];
    resultToChar(result, filename, sizeof(filename));
    char fullPath[512];

    snprintf(fullPath, sizeof(fullPath), "%s\\ShogunSync\\saves\\%s.z", G_LOCALAPPDATA, filename);
    struct stat buffer;

    int exists = stat(fullPath, &buffer);
    if(exists!=0) kw_compress(buff,ML,fullPath);
    
    free(buff);
}

bool init() {
    char syncPath[512]; 
    char objPath[512]; 
    char refPath[512];

    snprintf(syncPath, sizeof(syncPath), "%s\\The Creative Assembly\\Shogun2\\.sync", G_APPDATA);
    snprintf(objPath, sizeof(objPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects", G_APPDATA);
    snprintf(refPath, sizeof(refPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\refs", G_APPDATA);

    int result;

    struct stat st = {0};
    if (stat(syncPath, &st) == 0) {
        printf("DIRECTORY syncPath ALREADY EXISTS\n");
    }
    else{
#if defined(_WIN32)
    result = _mkdir(syncPath);
#else 
    result = mkdir(syncPath, 0700); 
#endif

    if (result != 0) {
        return 1;
    }

#if defined(_WIN32)
    SetFileAttributesA(syncPath, FILE_ATTRIBUTE_HIDDEN);
#endif
    }
    if (stat(objPath, &st) == 0) {
        printf("DIRECTORY ALREADY EXISTS\n");

    }
    else{
#if defined(_WIN32)
    result = _mkdir(objPath);
#else 
    result = mkdir(objPath, 0700); 
#endif

    if (result != 0) {
        return 1;
    }

#if defined(_WIN32)
    SetFileAttributesA(objPath, FILE_ATTRIBUTE_HIDDEN);
#endif
    }
    if (stat(refPath, &st) == 0) {
        printf("DIRECTORY ALREADY EXISTS\n");

    }
    else{
#if defined(_WIN32)
    result = _mkdir(refPath);
#else 
    result = mkdir(refPath, 0700); 
#endif

    if (result != 0) {
        return 1;
    }

#if defined(_WIN32)
    SetFileAttributesA(refPath, FILE_ATTRIBUTE_HIDDEN);
#endif
    }
    return 0;
}

bool isStorageInitialized(void){
    if (G_APPDATA == NULL) {
        getVariables();
    }
    if (G_APPDATA == NULL) {
        return false;
    }
    char syncPath[512];
    snprintf(syncPath, sizeof(syncPath), "%s\\The Creative Assembly\\Shogun2\\.sync", G_APPDATA);
    DWORD attr = GetFileAttributesA(syncPath);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool scan(mapTree **mt, int *x){
    char savePath[512];
    snprintf(savePath, sizeof(savePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\*.save_multiplayer", G_APPDATA);
    
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(savePath, &findFileData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        *x = 0;
        return 0;
    }

    size_t f = *x;
    *x = 0;
    
    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            (*x)++;
        }
    } while (FindNextFileA(hFind, &findFileData) != 0);
    FindClose(hFind);

    if (mt != NULL && f == 0 && *x > 0) {
        *mt = malloc(sizeof(mapTree)*(*x));
        hFind = FindFirstFileA(savePath, &findFileData);
        if (hFind == INVALID_HANDLE_VALUE) {
            printf("Error 008: Directory scan failed\n");
            return 1;
        }
        uint16_t i = 0;
        do {
            if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (i >= *x) break; 
                
                char* buf = strdup(findFileData.cFileName);
                (*mt)[i].name = buf;
                (*mt)[i].type = "blob";
                i++;
            }
        } while (FindNextFileA(hFind, &findFileData) != 0);
        FindClose(hFind);
    }
    return 0;
}

bool getActiveCampaignSave(char *saveNameOut, size_t saveNameSize, char hashOut[41]){
    if (G_APPDATA == NULL) {
        getVariables();
    }
    if (G_APPDATA == NULL) {
        return 1;
    }

    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\*.save_multiplayer", G_APPDATA);

    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 1;
    }

    char bestName[MAX_PATH] = "";
    FILETIME bestTime = {0, 0};
    bool found = false;

    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!found || CompareFileTime(&findFileData.ftLastWriteTime, &bestTime) > 0) {
                bestTime = findFileData.ftLastWriteTime;
                strncpy(bestName, findFileData.cFileName, sizeof(bestName) - 1);
                bestName[sizeof(bestName) - 1] = '\0';
                found = true;
            }
        }
    } while (FindNextFileA(hFind, &findFileData) != 0);
    FindClose(hFind);

    if (!found) {
        return 1;
    }

    if (saveNameOut != NULL && saveNameSize > 0) {
        strncpy(saveNameOut, bestName, saveNameSize - 1);
        saveNameOut[saveNameSize - 1] = '\0';
    }

    char fullSavePath[512];
    snprintf(fullSavePath, sizeof(fullSavePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s", G_APPDATA, bestName);

    FILE *f = NULL;
    for (int r = 0; r < 5; r++) {
        f = fopen(fullSavePath, "rb");
        if (f != NULL) break;
        Sleep(100);
    }
    if (f == NULL) {
        return 1;
    }

    fseek(f, 0L, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    uint8_t *buff = malloc(size);
    if (buff == NULL) {
        fclose(f);
        return 1;
    }
    fread(buff, 1, size, f);
    fclose(f);

    uint32_t result[5];
    sha1(buff, size, result);
    free(buff);

    resultToChar(result, hashOut, 41);
    return 0;
}

bool add(char hashTree[41]){
    char activeName[MAX_PATH] = "";
    if (getActiveCampaignSave(activeName, sizeof(activeName), hashTree) == 0) {
        strcpy(latestTree, hashTree);
        if (shared != NULL) {
            strncpy(shared->lastSaveName, activeName, sizeof(shared->lastSaveName) - 1);
            shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
        }

        char fullSavePath[512];
        snprintf(fullSavePath, sizeof(fullSavePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s", G_APPDATA, activeName);
        FILE *save = fopen(fullSavePath, "rb");
        if (save != NULL) {
            fseek(save, 0L, SEEK_END);
            size_t ML = ftell(save);
            rewind(save);
            uint8_t *buff = malloc(ML);
            if (buff != NULL) {
                fread(buff, 1, ML, save);
                fclose(save);
                char fullObjPath[512];
                snprintf(fullObjPath, sizeof(fullObjPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.z", G_APPDATA, hashTree);
                kw_compress(buff, ML, fullObjPath);
                free(buff);
            } else {
                fclose(save);
            }
        }
        return 0;
    }

    mapTree *mt = NULL;
    int x = 0;
    bool result = scan(&mt,&x);
    uint64_t mapSize = 0;
    if (result == 0)
    {
        char savePath[512];
        for (size_t i = 0; i < x; i++)
        {
            snprintf(savePath, sizeof(savePath),
            "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s", G_APPDATA,mt[i].name);
            FILE *save = NULL;
            for (int r = 0; r < 5; r++) {
                save = fopen(savePath, "rb");
                if (save != NULL) break;
                Sleep(100);
            }
            if (save == NULL) {
                printf("Error 001: Save file not found\n");
                for (size_t j = 0; j < i; j++) {
                    free(mt[j].name);
                    free(mt[j].hash);
                }
                free(mt);
                return 1;
            }

            fseek(save, 0L, SEEK_END);
            size_t ML = ftell(save);
            rewind(save);

            uint8_t *buff = malloc(ML);
            if (buff == NULL) {
                fclose(save);
                printf("Error 017: Memory allocation failed\n");
                for (size_t j = 0; j < i; j++) {
                    free(mt[j].name);
                    free(mt[j].hash);
                }
                free(mt);
                return 1;
            }
            fread(buff,1,ML,save);
            uint32_t result[5];
            sha1(buff,ML,result);

            fclose(save);

            char filename[41];
            resultToChar(result, filename, sizeof(filename));
            char fullPath[512];

            snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.z", G_APPDATA, filename);

            kw_compress(buff,ML,fullPath);

            free(buff);

            char* buf = strdup(filename);
            mt[i].hash = buf;

            mapSize += 48 + strlen(mt[i].name); 
        }
        mapSize += 1;
        uint8_t *buff = malloc(mapSize * sizeof(uint8_t));
        if (buff == NULL) {
            printf("Error 017: Memory allocation failed\n");
            for (size_t j = 0; j < x; j++) {
                free(mt[j].name);
                free(mt[j].hash);
            }
            free(mt);
            return 1;
        }
        int offset = 0;
        for (size_t i = 0; i < x; i++)
        {
            offset += snprintf((char*)buff + offset, mapSize - offset, "%s %s %s \n",
            mt[i].type,mt[i].hash,mt[i].name);
        }

        uint32_t result[5];
        sha1(buff,offset,result);
        
        char filename[41];
        resultToChar(result, filename, sizeof(filename));
        char fullPath[512];

        snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.m", G_APPDATA, filename);

        kw_compress(buff,offset,fullPath);

        strcpy(hashTree,filename);
        if (latestActiveHash[0] == '\0') {
            strcpy(latestTree,filename);
        }
        free(buff);
    }
    else{
        printf("Error 008: Directory scan failed\n");
        return 1;
    }   
    for (size_t i = 0; i < x; i++)
    {
        free(mt[i].name);
        free(mt[i].hash);
    }
    free(mt);
    return 0;
}

bool commit(char comment[]){
    char hashTree[41];
    bool f = add(hashTree);
    if (f == 0)
    {
        if (latestActiveHash[0] == '\0') {
            strcpy(latestTree, hashTree);
        }
        size_t malSize = sizeof(char) * (4 + 1 + 41 + 1 + 6 + 1 + 256 + 1 + 7 + 1 + strlen(comment))+2;
        char *buff = malloc(malSize);
        if(buff != NULL){
            snprintf((char*)buff, malSize, "Tree %s Author %s Comment %s\n",hashTree,hostname,comment);

            uint32_t result[5];
            sha1(buff,strlen(buff),result);

            char filename[41];
            resultToChar(result, filename, sizeof(filename));
            char fullPath[512];

            snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.c", G_APPDATA, filename);

            kw_compress(buff,strlen(buff),fullPath);

            char latestPath[512];
            snprintf(latestPath, sizeof(latestPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\refs\\latest", G_APPDATA);
            FILE *lp = fopen(latestPath,"w");
            if (lp == NULL) {
                printf("Error 018: Reference update failed\n");
            }
            else{
                fprintf(lp, "%s", filename);
                fclose(lp);
                strcpy(latest,filename);
            }

            free(buff);
            return 0;
        }

    }
    return 1;
}

char* getLatest(){
    return "NULL";
}

bool watch(){
    HANDLE hd;
    char savePath[512];
    bool flag = 0;
    snprintf(savePath, sizeof(savePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer", G_APPDATA);
    hd = FindFirstChangeNotificationA(savePath, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);

    initIPC();

    while (running)
    {
        if (connection)
        {
            pollNetwork();
        }
        else
        {
            Sleep(50);
        }

        static DWORD lastInjectCheck = 0;
        DWORD now = GetTickCount();
        if (now - lastInjectCheck >= 1000) {
            lastInjectCheck = now;
            autoInjectShogun(NULL);
        }

        if (shared != NULL) {
            shared->lastHeartbeatTick = now;
        }

        DWORD status = WaitForSingleObject(hd, 0);
        
        if (status == WAIT_OBJECT_0) {
            FindNextChangeNotification(hd);
            while (WaitForSingleObject(hd, 150) == WAIT_OBJECT_0) {
                FindNextChangeNotification(hd);
            }

            if (g_isRestoringSave || (GetTickCount() - g_lastRestoreTick < 2500)) {
                continue;
            }

            char activeName[MAX_PATH];
            char currentTree[41];
            if (getActiveCampaignSave(activeName, sizeof(activeName), currentTree) != 0) {
                continue;
            }

            if (strlen(g_previousSaveHash) > 0 && strcmp(currentTree, g_previousSaveHash) == 0) {
                continue;
            }

            strcpy(g_previousSaveHash, currentTree);
            strcpy(latestActiveHash, currentTree);
            strcpy(latestTree, currentTree);

            if (shared != NULL) {
                strncpy(shared->lastSaveName, activeName, sizeof(shared->lastSaveName) - 1);
                shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
            }

            printf("New tour...\n");
            flag = commit("Auto-tura");
            if (flag == 1) running = false;
            strcpy(latestTree, currentTree);

            if (isHost) {
                tour++;
                if (shared != NULL) {
                    shared->tour = tour;
                    shared->status = SYNC_STATUS_OK;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Synchronized (Tour %d)", tour);
                }
                snprintf(msg, sizeof(msg), "%s|%i|%s", latestTree, tour, activeName);
                sendPackage(msg, strlen(msg));
            }
            else {
                if (lastHostTour > tour) {
                    tour = lastHostTour;
                } else {
                    tour++;
                }
                if (shared != NULL) {
                    shared->tour = tour;
                }
                if (lastHostTour == tour) {
                    int diff = strcmp(latestActiveHash, lastHostHash);
                    if (diff == 0) {
                        printf("Saves are sync\n");
                        if (shared != NULL) {
                            shared->status = SYNC_STATUS_OK;
                            snprintf(shared->status_msg, sizeof(shared->status_msg), "Synchronized (Tour %d)", tour);
                        }
                    }
                    else {
                        printf("Saves are desync\n");
                        if (shared != NULL) {
                            shared->status = SYNC_STATUS_DESYNC;
                            snprintf(shared->status_msg, sizeof(shared->status_msg), "Desync detected! Resyncing...");
                        }
                        printf("Resync: Requesting save from host...\n");
                        char req[128];
                        const char *tgt = (shared != NULL && shared->lastSaveName[0] != '\0') ? shared->lastSaveName : "autosave.save_multiplayer";
                        snprintf(req, sizeof(req), "REQ_SAVE|%s", tgt);
                        sendPackage(req, strlen(req));
                    }
                } else {
                    if (shared != NULL) {
                        shared->status = SYNC_STATUS_OK;
                        snprintf(shared->status_msg, sizeof(shared->status_msg), "Turn %d saved - waiting for host", tour);
                    }
                }
            }
        }
    }
    closeIPC();
    FindCloseChangeNotification(hd);
    return flag;
}

bool initIPC(){
    if (shared != NULL) {
        return 0;
    }
    shogunHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(ShogunSharedData), "Local\\ShogunSyncIPC");
    bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
    if (shogunHandle == NULL) {
        shogunHandle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Local\\ShogunSyncIPC");
        if (shogunHandle == NULL) {
            printf("Error 021: Shared memory initialization failed\n");
            return 1;
        }
        alreadyExists = true;
    }

    shared = (ShogunSharedData *)MapViewOfFile(shogunHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShogunSharedData));
    if (shared == NULL) {
        printf("Error 022: Memory view mapping failed\n");
        CloseHandle(shogunHandle);
        shogunHandle = NULL;
        return 1;
    }

    if (!alreadyExists) {
        memset(shared, 0, sizeof(ShogunSharedData));
        shared->status = SYNC_STATUS_CONNECTING;
        shared->tour = 0;
        snprintf(shared->status_msg, sizeof(shared->status_msg), "Waiting for game...");
    }
    return 0;
}

void closeIPC(){
    if (shared != NULL) {
        UnmapViewOfFile(shared);
        shared = NULL;
    }
    if (shogunHandle != NULL) {
        CloseHandle(shogunHandle);
        shogunHandle = NULL;
    }
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
            printf("Program stopped\n");
            closeIPC();
            running = false;
            return TRUE;
            
        default:
            return FALSE;
    }
}