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
