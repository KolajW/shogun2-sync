#include "kw_conf.h"
#define VERSION "0.0.2"
#define AUTHOR "KW"

int main(int argc, char *argv[]){
    //Info
    if( argc < 2){
        printf("Failed: No arguments\n");
        printf("Use: shogun-sync [args]\n");
        printf("Type: 'shogun-sync --help' for more informations\n");
        return 1;
    }
    //Version
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)
    {
        printf("Shogun Sync version: %s\n", VERSION);
        printf("Author: %s\n", AUTHOR);
        return 0;
    }
    //TEST
    if (strcmp(argv[1], "--test") == 0 || strcmp(argv[1], "-t") == 0)
    {
        printf("Finding and compressing save:\n");
        if(argc > 2){
            test(argv[2]);
        }
        else{
            test("autosave");
        }
        return 0;
    }
    //INIT
    if (strcmp(argv[1], "init") == 0)
    {
        bool result = init();
        if (result == 0)
        {
            printf("Initialization success\n");
        }
        else{
            printf("Error 007 INIT FAILED\n");
            return 1;
        }   
        return 0;
    }
    

    return 0;
}

