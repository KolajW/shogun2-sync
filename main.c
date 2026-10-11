#include "kw_conf.h"
#include <time.h>
#define VERSION "0.1.0-beta"
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
        printf("Error 009: Control handler failed\n");
        return 1;
    }
    //Info
    if( argc < 2){
        printf("Error 020: Command line argument missing\n");
        printf("Use: shogun-sync [args]\n");
        printf("Type: 'shogun-sync --help' for more informations\n");
        return 1;
    }
    //Help
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    {
        printf("Usage: shogun-sync [command] [options]\n\n");
        printf("Commands:\n");
        printf("  init                       Initialize synchronization directories\n");
        printf("  create <password>          Create a multiplayer room with a generated code\n");
        printf("  join <room_code> <pass>    Join an existing multiplayer room\n");
        printf("  commit                     Create a local save state commit\n");
        printf("  --watch, -w                Watch save directory for changes\n");
        printf("  --scan, -s                 Scan and count save files\n");
        printf("  --version, -v              Display version information\n");
        printf("  --help, -h                 Display help information\n");
        return 0;
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
        printf("Error 005: Variable initialization failed\n");
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
            printf("Error 007: Initialization failed\n");
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
            printf("Error 008: Directory scan failed\n");
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
            printf("Error 008: Directory scan failed\n");
            return 1;
        }  
        for (size_t i = 0; i < x; i++)
        {
            printf("%s\n", mt[i].name);
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
            printf("Error 018: Commit creation failed\n");
        }
        return 0;
    }

    //Watch for autosave change
    if (strcmp(argv[1], "--watch") == 0 || strcmp(argv[1], "-w") == 0)
    {
        printf("Use CTRL + C to exit\n");
        bool f = watch();
        if(f == 0){
            if (!running) {
                printf("Session closed\n");
            } else {
                printf("Success\n");
            }
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
            printf("Error 020: Password required\n");
            printf("Use: shogun-sync create [password]\n");
            return 1;
        }

        isHost = true;
        bool f = commit("Room commit");
        if(f == 0){
            printf("Success in commit\n");
        }
        else{
            printf("Error 018: Commit creation failed\n");
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
            if (!running) {
                printf("Session closed\n");
            } else {
                printf("Success\n");
            }
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
                printf("Error 018: Commit creation failed\n");
                return 1;
            }

            mg_log_level = MG_LL_NONE;
            mg_log_set(MG_LL_NONE);
            connectToRoom(argv[2], argv[3], "join");
            connection = true;
            printf("Use CTRL + C to exit\n");
            f = watch();
            if(f == 0){
                if (!running) {
                    printf("Session closed\n");
                } else {
                    printf("Success\n");
                }
            }
            else{
                printf("Failed\n");
            }
        }
        else
        {
            printf("Error 020: Room code and password required\n");
            printf("Use: shogun-sync join [room_code] [password]\n");
            return 1;
        }

        return 0;
    }

    return 0;
}