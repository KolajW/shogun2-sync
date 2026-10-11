#include "kw_conf.h"
#include <time.h>
#define VERSION "0.0.8"
#define AUTHOR "KW"

void generateRoomCode(char *code, size_t len) {
    const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (size_t i = 0; i < len - 1; i++) {
        code[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    code[len - 1] = '\0';
}

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
        return 0;
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
            return 1;
        }
        return 0;
    }

    //Create room
    if (strcmp(argv[1], "create") == 0)
    {
        if (argc < 3) {
            printf("Failed: Password required\n");
            printf("Use: shogun-sync create [password]\n");
            return 1;
        }

        isHost = true;
        bool f = commit("Room commit");
        if(f == 0){
            printf("Success in commit\n");
        }
        else{
            printf("Failed commit\n");
            return 1;
        }

        srand((unsigned int)time(NULL));
        char roomCode[7];
        generateRoomCode(roomCode, sizeof(roomCode));

        printf("Room code: %s\n", roomCode);
        printf("Password: %s\n", argv[2]);

        mg_log_level = MG_LL_NONE;
        mg_log_set(MG_LL_NONE);
        connectToRoom(roomCode, argv[2], "create");
        connection = true;
        printf("Use CTRL + C to exit\n");
        f = watch();
        if(f == 0){
            printf("Success\n");
        }
        else{
            printf("Failed\n");
        }
        return 0;
    }

    //Join to room
    if (strcmp(argv[1], "join") == 0)
    {
        if (argc > 3)
        {
            bool f = commit("Join commit");
            if(f == 0){
                printf("Success in commit\n");
            }
            else{
                printf("Failed commit\n");
                return 1;
            }

            mg_log_level = MG_LL_NONE;
            mg_log_set(MG_LL_NONE);
            connectToRoom(argv[2], argv[3], "join");
            connection = true;
            printf("Use CTRL + C to exit\n");
            f = watch();
            if(f == 0){
                printf("Success\n");
            }
            else{
                printf("Failed\n");
            }
        }
        else
        {
            printf("Failed: Room code and password required\n");
            printf("Use: shogun-sync join [room_code] [password]\n");
            return 1;
        }

        return 0;
    }

    return 0;
}