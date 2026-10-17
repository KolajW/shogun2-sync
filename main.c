#include "kw_conf.h"
#include "kw_gui.h"
#include "kw_update.h"
#include <time.h>
#define VERSION "0.1.1-beta"
#define AUTHOR "KW"

void generateRoomCode(char *code, size_t len) {
    const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (size_t i = 0; i < len - 1; i++) {
        code[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    code[len - 1] = '\0';
}

int main(int argc, char *argv[]){
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        printf("Error 009: Control handler failed\n");
        return 1;
    }

    if( argc < 2 || strcmp(argv[1], "--gui") == 0 || strcmp(argv[1], "-g") == 0){
        FreeConsole();
        bool condition = getVariables();
        if (condition == 1) {
            printf("Error 005: Variable initialization failed\n");
            MessageBoxA(NULL, "Error 005: Variable initialization failed", "Shogun Sync", MB_ICONERROR | MB_OK);
            return 1;
        }
        int guiRes = launchGUI();
        if (guiRes != 0) {
            MessageBoxA(NULL, "Failed to initialize DirectX 11 interface.", "Shogun Sync", MB_ICONERROR | MB_OK);
        }
        return guiRes;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    {
        printf("Usage: shogun-sync [command] [options]\n\n");
        printf("Commands:\n");
        printf("  --gui, -g                  Launch graphical user interface\n");
        printf("  init                       Initialize synchronization directories\n");
        printf("  create <password>          Create a multiplayer room with a generated code\n");
        printf("  join <room_code> <pass>    Join an existing multiplayer room\n");
        printf("  commit                     Create a local save state commit\n");
        printf("  --watch, -w                Watch save directory for changes\n");
        printf("  --scan, -s                 Scan and count save files\n");
        printf("  inject [dll_path]          Inject overlay DLL into running Shogun 2 process\n");
        printf("  reload [save_name]         Trigger in-game save reload via overlay\n");
        printf("  update, -u                 Check and install updates\n");
        printf("  uninstall                  Uninstall Shogun-Sync from system\n");
        printf("  --version, -v              Display version information\n");
        printf("  --help, -h                 Display help information\n");
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)
    {
        printf("Shogun Sync version: %s\n", VERSION);
        printf("Author: %s\n", AUTHOR);
        return 0;
    }

    bool condition = getVariables();
    if (condition == 1)
    {
        printf("Error 005: Variable initialization failed\n");
        return 1;
    }

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

    if (strcmp(argv[1], "inject") == 0)
    {
        DWORD pid = findShogunProcess();
        if (pid == 0) {
            printf("Error 023: Shogun 2 process not found\n");
            return 1;
        }
        const char *dll = (argc > 2) ? argv[2] : (isProcess64Bit(pid) ? "shogun_overlay64.dll" : "shogun_overlay32.dll");
        bool err = injectDLL(pid, dll);
        return err ? 1 : 0;
    }

    if (strcmp(argv[1], "reload") == 0 || strcmp(argv[1], "--reload") == 0)
    {
        if (initIPC() != 0 || shared == NULL) {
            printf("Error 022: IPC shared memory not available\n");
            return 1;
        }
        const char *sName = (argc > 2) ? argv[2] : "autosave.save_multiplayer";
        strncpy(shared->lastSaveName, sName, sizeof(shared->lastSaveName) - 1);
        shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
        shared->triggerReload = true;
        snprintf(shared->status_msg, sizeof(shared->status_msg), "Manual reload requested: %s", sName);
        printf("Success: Save reload signal dispatched to in-game overlay (%s)\n", sName);
        return 0;
    }

    if (strcmp(argv[1], "update") == 0 || strcmp(argv[1], "--update") == 0 || strcmp(argv[1], "-u") == 0)
    {
        return updateApplication() ? 1 : 0;
    }

    if (strcmp(argv[1], "uninstall") == 0 || strcmp(argv[1], "--uninstall") == 0)
    {
        return uninstallApplication() ? 1 : 0;
    }

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

        initIPC();
        if (shared != NULL){
            strncpy(shared->room_code, roomCode, sizeof(shared->room_code) - 1);
            shared->room_code[sizeof(shared->room_code) - 1] = '\0';
            shared->isHost = true;
        }
        

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

            initIPC();
            if (shared != NULL){
                strncpy(shared->room_code, argv[2], sizeof(shared->room_code) - 1);
                shared->room_code[sizeof(shared->room_code) - 1] = '\0';
                shared->isHost = false;
            }

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