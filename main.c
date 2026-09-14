#include "kw_conf.h"
#define VERSION "0.0.4"
#define AUTHOR "KW"

int main(int argc, char *argv[]){
    //Check exit command
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        printf("Error: Could not set control handler\n");
        return 1;
    }
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
    //Variables for next functions
    bool condition = getVariables();
    if (condition == 1)
    {
        printf("Failed: Variables issue\n");
        return 1;
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
    //Scan
    if (strcmp(argv[1], "--scan") == 0 || strcmp(argv[1], "-s") == 0)
    {
        int x = 1;
        bool result = scan(NULL,&x);
        if (result == 0)
        {
            printf("Scan completed\n");
        }
        else{
            printf("Error scan failed\n");
            return 1;
        }   
        return 0;
    }

    //Test Scan
    if (strcmp(argv[1], "--tscan") == 0 || strcmp(argv[1], "-ts") == 0)
    {
        mapTree *mt = NULL;
        int x = 0;
        bool result = scan(&mt,&x);
        if (result == 0)
        {
            printf("Scan completed\n");
        }
        else{
            printf("Error scan failed\n");
            return 1;
        }  
        for (size_t i = 0; i < x; i++)
        {
            printf(mt[i].name);
            printf("\n");
            free(mt[i].name);
        }
        

        free(mt);
        return 0;
    }

    //Commit
    if (strcmp(argv[1], "commit") == 0)
    {
        bool f = commit("First commit");
        if(f == 0){
            printf("Success\n");
        }
        else{
            printf("Failed\n");
        }
    }

    //Watch for autosave change
    if (strcmp(argv[1], "--watch") == 0 || strcmp(argv[1], "-w") == 0)
    {
        printf("Use CTRL + C to exit\n");
        bool f = watch();
        if(f == 0){
            printf("Success\n");
        }
        else{
            printf("Failed\n");
        }
    }
    

    

    return 0;
}

