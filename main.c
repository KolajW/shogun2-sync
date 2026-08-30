#include "kw_compress.h"

int main(){
    //Open file
    FILE *save = fopen("C:\\Users\\szynk\\AppData\\Roaming\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\autosave.save_multiplayer","rb");
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
    
    snprintf(fullPath, sizeof(fullPath), "%s.z", filename);

    kw_compress(buff,ML,fullPath);

    //Free the RAM!
    free(buff);

    return 0;
}

