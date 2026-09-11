#include "kw_conf.h"

void test(char saveName[]){
    char *appdata = getenv("APPDATA");
    if (appdata == NULL) {
        printf("Error 005 APPDATA PATH NOT FOUND:\n");
        return;
    }

    char savePath[512];
    snprintf(savePath, sizeof(savePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s.save_multiplayer", appdata,saveName);
    

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
    
    char *loc = getenv("LOCALAPPDATA");
    if (loc == NULL) {
        printf("Error 006 PROGRAM PATH NOT FOUND:\n");
        return;
    }

    snprintf(fullPath, sizeof(fullPath), "%s\\ShogunSync\\saves\\%s.z", loc, filename);

    kw_compress(buff,ML,fullPath);

    //Free the RAM!
    free(buff);
}

bool init() {
    char syncPath[512]; 
    char objPath[512]; 
    char refPath[512];
    char *appdata = getenv("APPDATA");
    
    if (appdata == NULL) {
        printf("Error 005 APPDATA PATH NOT FOUND\n");
        return 1;
    }

    snprintf(syncPath, sizeof(syncPath), "%s\\The Creative Assembly\\Shogun2\\.sync", appdata);
    snprintf(objPath, sizeof(objPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\objects", appdata);
    snprintf(refPath, sizeof(refPath), "%s\\The Creative Assembly\\Shogun2\\.sync\\refs", appdata);

    struct stat st = {0};
    if (stat(syncPath, &st) == 0) {
        printf("DIRECTORY ALREADY EXISTS\n");
        return 1; 
    }
    if (stat(objPath, &st) == 0) {
        printf("DIRECTORY ALREADY EXISTS\n");
        return 1; 
    }
    if (stat(refPath, &st) == 0) {
        printf("DIRECTORY ALREADY EXISTS\n");
        return 1; 
    }
    int result;
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

    return 0;
}
