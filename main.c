#include "kw_sha1.h"


char* test_hash_c(char path[]){
    FILE *save;
    save = fopen(path, "rb");
    if (save == NULL)
    {
        printf("Blad pliku\n");
        return NULL;
    }

    fclose(save);

    return 0;
}



int main(){
    
    
    
    /*char *buf = test_hash_c("C:/Users/szynk/AppData/Roaming/The Creative Assembly/Shogun2/save_games_multiplayer/autosave.save_multiplayer");
    if(buf != NULL){
        SHA1_CTX sha; uint8_t results[20]; int n;

        n = strlen(buf); SHA1Init(&sha); SHA1Update(&sha, (uint8_t *)buf, n); SHA1Final(results, &sha);
        printf("0x"); for (n = 0; n < 20; n++)

        printf("%02x", results[n]);
        putchar('n');
    }*/

    printf("Hello World!\n");
    sha1();
    return 0;
}

