#include "kw_conf.h"

char *G_APPDATA;
char *G_LOCALAPPDATA;
volatile bool running = true;
bool connection = false;

//Needed variables
bool getVariables(){
    G_APPDATA = getenv("APPDATA");
    if (G_APPDATA == NULL) {
        printf("Error 005 APPDATA PATH NOT FOUND:\n");
        return 1;
    }

    G_LOCALAPPDATA = getenv("LOCALAPPDATA");
    if (G_LOCALAPPDATA == NULL) {
        printf("Error 006 PROGRAM PATH NOT FOUND:\n");
        return 1;
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
        printf("Error 001 SAVE NOT FOUND\n");
        return;
    }

    //Save size
    fseek(save, 0L, SEEK_END);
    size_t ML = ftell(save);
    rewind(save);

    //Save File into RAM
    uint8_t *buff = malloc(ML);
    fread(buff,1,ML,save);
    //Array for SHA1
    uint32_t result[5];
    sha1(buff,ML,result);

    //Closing file
    fclose(save);

    printS(result);
    char filename[41];
    resultToChar(result, filename, sizeof(filename));
    char fullPath[256];

    snprintf(fullPath, sizeof(fullPath), "%s\\ShogunSync\\saves\\%s.z", G_LOCALAPPDATA, filename);

    kw_compress(buff,ML,fullPath);

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

    snprintf(savePath, sizeof(savePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer", G_APPDATA);
    struct dirent *de;
    DIR *dr = opendir(savePath);
    if (dr == NULL)
    {
        printf("Error DIRECTORY CAN NOT BE OPENED" );
        return 1;
    }

    const char *ext = ".save_multiplayer";
    size_t ext_len = strlen(ext);
    size_t f = *x;
    *x = 0;
    while ((de = readdir(dr)) != NULL){
        size_t name_len = strlen(de->d_name);
        if (name_len >= ext_len) {
            if (strcmp(de->d_name + name_len - ext_len, ext) == 0) {
                printf("%s\n", de->d_name);
                (*x)++;
            }
        }
        
    }
    closedir(dr);
    //Load to struct
    if(f == 0){
        dr = opendir(savePath);
        *mt = malloc(sizeof(mapTree)*(*x));
        if (dr == NULL)
        {
            printf("Error DIRECTORY CAN NOT BE OPENED" );
            return 1;
        }
        uint16_t i = 0;
        while ((de = readdir(dr)) != NULL){
            size_t name_len = strlen(de->d_name);
            if (name_len >= ext_len) {
                if (strcmp(de->d_name + name_len - ext_len, ext) == 0) {
                    char* buf = strdup(de->d_name);
                    (*mt)[i].name = buf;
                    (*mt)[i].type = "blob";
                    i++;
                }
                
            }
            
        }
        closedir(dr);
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
            FILE *save = fopen(savePath,"rb");
            //Possible error
            if (save == NULL) {
                printf("Error 001 SAVE NOT FOUND\n");
                return 1;
            }

            //Save size
            fseek(save, 0L, SEEK_END);
            size_t ML = ftell(save);
            rewind(save);

            //Save File into RAM
            uint8_t *buff = malloc(ML);
            fread(buff,1,ML,save);
            //Array for SHA1
            uint32_t result[5];
            sha1(buff,ML,result);

            //Closing file
            fclose(save);

            printS(result);

            char filename[41];
            resultToChar(result, filename, sizeof(filename));
            char fullPath[256];

            snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.z", G_APPDATA, filename);

            kw_compress(buff,ML,fullPath);

            //Free the RAM!
            free(buff);

            //For map hashed name of file

            char* buf = strdup(filename);
            mt[i].hash = buf;

            //Size of the map for malloc
            //Blob 4 chars
            mapSize += 4;
            //Space
            mapSize += 1;
            //Size of hash - always 41 chars
            mapSize += 41;
            //Space
            mapSize += 1;
            //Size of save name
            mapSize += strlen(mt[i].name);
            //Enter
            mapSize += 1;
        }
        //End of text \0
        mapSize += 1;
        uint8_t *buff = malloc(mapSize * sizeof(uint8_t));
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
        char fullPath[256];

        snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.m", G_APPDATA, filename);

        kw_compress(buff,offset,fullPath);

        strcpy(hashTree,filename);
        //Clear the RAM
        free(buff);
    }
    else{
        printf("Error scan failed\n");
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
    char hostname[256];
    if (f == 0)
    {
        size_t malSize = sizeof(char) * (4 + 1 + 41 + 1 + 6 + 1 + 256 + 1 + 7 + 1 + strlen(comment))+2;
        char *buff = malloc(malSize);
        if(buff != NULL){
            #ifdef _WIN32
                WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                printf("Winsock initialization failed.\n");
                free(buff);
                return 1;
            }
            #endif
            if (gethostname(hostname, sizeof(hostname)) == 0) {
                snprintf((char*)buff, malSize, "Tree %s Author %s Comment %s\n",hashTree,hostname,comment);

                //Hash commit
                uint32_t result[5];
                sha1(buff,strlen(buff),result);

                char filename[41];
                resultToChar(result, filename, sizeof(filename));
                char fullPath[256];

                snprintf(fullPath, sizeof(fullPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects\\%s.c", G_APPDATA, filename);

                kw_compress(buff,strlen(buff),fullPath);

                char latestPath[256];
                snprintf(latestPath, sizeof(latestPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\refs\\latest", G_APPDATA);
                FILE *lp = fopen(latestPath,"w");
                //Possible error
                if (lp == NULL) {
                    printf("Error 001 SAVE NOT FOUND\n");
                }
                else{
                    fprintf(lp, "%s", filename);
                    fclose(lp);
                }
            }

            //Cleaning
            #ifdef _WIN32
                WSACleanup();
            #endif
            free(buff);
            return 0;
        }

    }
    return 1;
    
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
        
        DWORD status = WaitForSingleObject(hd, 10);
        
        if (status == WAIT_OBJECT_0) {
            printf("New tour...\n");
            Sleep(50);
            flag = commit("Auto-tura");
            if(flag == 1) running = false;
            FindNextChangeNotification(hd); 
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