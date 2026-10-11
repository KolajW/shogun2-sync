#include "kw_conf.h"

char *G_APPDATA;
char *G_LOCALAPPDATA;
volatile bool running = true;
bool connection = false;
bool isHost = false;
char hostname[256];

int tour = 0;
char latest[41];
char msg[60];

//Needed variables
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
    
    //Open file
    FILE *save = fopen(savePath,"rb");
    //Possible error
    if (save == NULL) {
        printf("Error 001: Save file not found\n");
        return;
    }

    //Save size
    fseek(save, 0L, SEEK_END);
    size_t ML = ftell(save);
    rewind(save);

    //Save File into RAM
    uint8_t *buff = malloc(ML);
    if (buff == NULL) {
        fclose(save);
        printf("Error 017: Memory allocation failed\n");
        return;
    }
    fread(buff,1,ML,save);
    //Array for SHA1
    uint32_t result[5];
    sha1(buff,ML,result);

    //Closing file
    fclose(save);

    printS(result);
    char filename[41];
    resultToChar(result, filename, sizeof(filename));
    char fullPath[512];

    snprintf(fullPath, sizeof(fullPath), "%s\\ShogunSync\\saves\\%s.z", G_LOCALAPPDATA, filename);
    struct stat buffer;

    int exists = stat(fullPath, &buffer);
    if(exists!=0) kw_compress(buff,ML,fullPath);
    
    //Free the RAM!
    free(buff);
}

bool init() {
    char syncPath[512]; 
    char objPath[512]; 
    char refPath[512];
    //char latestPath[512];

    snprintf(syncPath, sizeof(syncPath), "%s\\The Creative Assembly\\Shogun2\\.sync", G_APPDATA);
    snprintf(objPath, sizeof(objPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects", G_APPDATA);
    snprintf(refPath, sizeof(refPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\refs", G_APPDATA);
    //snprintf(latestPath, sizeof(latestPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\refs\\latest", G_APPDATA);

    int result;

    struct stat st = {0};
    if (stat(syncPath, &st) == 0) {
        printf("DIRECTORY syncPath ALREADY EXISTS\n");
    }
    else{
        //Main dir
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
        //Obj dir
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
        //Ref dir
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
    /*if (stat(latestPath, &st) == 0) {
        printf("DIRECTORY ALREADY EXISTS\n");

    }
    else{
        //Latest dir
#if defined(_WIN32)
    result = _mkdir(latestPath);
#else 
    result = mkdir(latestPath, 0700); 
#endif

    if (result != 0) {
        return 1;
    }

#if defined(_WIN32)
    SetFileAttributesA(latestPath, FILE_ATTRIBUTE_HIDDEN);
#endif
    }*/
    return 0;
}

//Scan all saves
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
            //printf("%s\n", findFileData.cFileName);
            (*x)++;
        }
    } while (FindNextFileA(hFind, &findFileData) != 0);
    FindClose(hFind);

    //Load to struct
    if(f == 0 && *x > 0){
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

//Adds saves and creates blobs with map
bool add(char hashTree[41]){
    mapTree *mt = NULL;
    int x = 0;
    bool result = scan(&mt,&x);
    //uint8_t *map = 
    uint64_t mapSize = 0;
    if (result == 0)
    {
        char savePath[512];
        for (size_t i = 0; i < x; i++)
        {
            snprintf(savePath, sizeof(savePath),
            "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s", G_APPDATA,mt[i].name);
            //Open file
            FILE *save = NULL;
            for (int r = 0; r < 5; r++) {
                save = fopen(savePath, "rb");
                if (save != NULL) break;
                Sleep(100);
            }
            //Possible error
            if (save == NULL) {
                printf("Error 001: Save file not found\n");
                for (size_t j = 0; j < i; j++) {
                    free(mt[j].name);
                    free(mt[j].hash);
                }
                free(mt);
                return 1;
            }

            //Save size
            fseek(save, 0L, SEEK_END);
            size_t ML = ftell(save);
            rewind(save);

            //Save File into RAM
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
            //Array for SHA1
            uint32_t result[5];
            sha1(buff,ML,result);

            //Closing file
            fclose(save);

            //printS(result);

            char filename[41];
            resultToChar(result, filename, sizeof(filename));
            char fullPath[512];

            snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.z", G_APPDATA, filename);

            kw_compress(buff,ML,fullPath);

            //Free the RAM!
            free(buff);

            //For map hashed name of file

            char* buf = strdup(filename);
            mt[i].hash = buf;

            //Size of the map for malloc
            //Blob 4 chars
            //mapSize += 4;
            //Space
            //mapSize += 1;
            //Size of hash - always 41 chars
            //mapSize += 41;
            //Space
            //mapSize += 1;
            //Size of save name
            //mapSize += strlen(mt[i].name);
            //Enter
            //mapSize += 1;
            mapSize += 48 + strlen(mt[i].name); 
        }
        //End of text \0
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

        //Hash map
        uint32_t result[5];
        sha1(buff,offset,result);
        
        char filename[41];
        resultToChar(result, filename, sizeof(filename));
        char fullPath[512];

        snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.m", G_APPDATA, filename);

        kw_compress(buff,offset,fullPath);

        strcpy(hashTree,filename);
        //Clear the RAM
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
    //char hostname[256];
    if (f == 0)
    {
        size_t malSize = sizeof(char) * (4 + 1 + 41 + 1 + 6 + 1 + 256 + 1 + 7 + 1 + strlen(comment))+2;
        char *buff = malloc(malSize);
        if(buff != NULL){
            snprintf((char*)buff, malSize, "Tree %s Author %s Comment %s\n",hashTree,hostname,comment);

            //Hash commit
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
            //Possible error
            if (lp == NULL) {
                printf("Error 018: Reference update failed\n");
            }
            else{
                fprintf(lp, "%s", filename);
                fclose(lp);
                strcpy(latest,filename);
            }

            //Cleaning
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
        

        DWORD status = WaitForSingleObject(hd, 0);
        
        if (status == WAIT_OBJECT_0) {
            printf("New tour...\n");
            Sleep(500);
            flag = commit("Auto-tura");
            if(flag == 1) running = false;
            FindNextChangeNotification(hd); 
            tour++;

            if(isHost){
                snprintf(msg, sizeof(msg), "%s|%i", latest,tour);
                sendPackage(msg,strlen(msg));
            }
            else{
                if (lastHostTour == tour) {
                    int diff = strcmp(latest, lastHostHash);
                    if (diff == 0) {
                        printf("Saves are sync\n");
                    }
                    else {
                        printf("Saves are desync\n");
                        printf("Resync: Requesting save from host...\n");
                        char req[64];
                        snprintf(req, sizeof(req), "REQ_SAVE|autosave.save_multiplayer");
                        sendPackage(req, strlen(req));
                    }
                }
            }
        }
    }
    FindCloseChangeNotification(hd);
    return flag;
    
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
            printf("Program stopped\n");
            running = false;
            return TRUE;
            
        default:
            return FALSE;
    }
}