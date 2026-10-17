#ifndef SHOGUN_IPC_H
#define SHOGUN_IPC_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SYNC_STATUS_DISCONNECTED = 0,
    SYNC_STATUS_CONNECTING   = 1,
    SYNC_STATUS_OK           = 2,
    SYNC_STATUS_DESYNC       = 3, 
    SYNC_STATUS_RESYNCING    = 4
} ShogunSyncStatus;

typedef struct {
    uint32_t tour;
    uint32_t status;
    char status_msg[64];
    char room_code[8];
    bool isHost;
    bool triggerReload;
    char lastSaveName[64];
    uint32_t reloadCount;
    uint32_t lastReloadTick;
    uint32_t lobbyUsers;
    uint32_t lobbyMaxUsers;
    uint32_t lastHeartbeatTick;
    bool shouldExit;
} ShogunSharedData;

#endif