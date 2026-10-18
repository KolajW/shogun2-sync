#include "kw_conn.h"
#include "kw_conf.h"
#include "kw_gui.h"
#include "zlib/zlib.h"
#include <time.h>

struct mg_connection *c = NULL;
struct mg_mgr mg;
static bool mg_initialized = false;
int lastHostTour = -1;
char lastHostHash[41] = {0};
char currentRoomCode[16] = {0};

static void patchMultiplayerSaveHeader(const char *savePath) {
    FILE *f = fopen(savePath, "r+b");
    if (!f) return;
    uint8_t hdr[0x100];
    size_t rd = fread(hdr, 1, sizeof(hdr), f);
    if (rd >= 0x88) {
        if (hdr[0x53] == 0x09 && hdr[0x74] == 0x09) {
            uint8_t tmp[8];
            memcpy(tmp, hdr + 0x54, 8);
            memcpy(hdr + 0x54, hdr + 0x75, 8);
            memcpy(hdr + 0x75, tmp, 8);
            fseek(f, 0, SEEK_SET);
            fwrite(hdr, 1, rd, f);
        } else {
            int p1 = -1, p2 = -1;
            for (size_t i = 0x30; i + 12 <= rd; i++) {
                if (hdr[i] == 0x09 && memcmp(hdr + i + 5, "\x01\x00\x10\x01", 4) == 0) {
                    if (p1 == -1) p1 = (int)(i + 1);
                    else if (p2 == -1) { p2 = (int)(i + 1); break; }
                }
            }
            if (p1 != -1 && p2 != -1) {
                uint8_t tmp[8];
                memcpy(tmp, hdr + p1, 8);
                memcpy(hdr + p1, hdr + p2, 8);
                memcpy(hdr + p2, tmp, 8);
                fseek(f, 0, SEEK_SET);
                fwrite(hdr, 1, rd, f);
            }
        }
    }
    fclose(f);
}

void sendSaveToClient(const char *saveName) {
    if (c == NULL) return;
    if (saveName == NULL || strstr(saveName, "..") != NULL || strchr(saveName, '/') != NULL || strchr(saveName, '\\') != NULL) {
        printf("Error 016: Invalid packet received\n");
        return;
    }
    char savePath[512];
    snprintf(savePath, sizeof(savePath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s", G_APPDATA, saveName);

    FILE *f = fopen(savePath, "rb");
    if (!f) {
        printf("Error 001: Save file not found: %s\n", savePath);
        return;
    }

    fseek(f, 0L, SEEK_END);
    size_t fileSize = ftell(f);
    rewind(f);

    uint8_t *fileBuf = malloc(fileSize);
    if (!fileBuf) {
        fclose(f);
        printf("Error 017: Memory allocation failed\n");
        return;
    }
    fread(fileBuf, 1, fileSize, f);
    fclose(f);

    uLongf compSize = compressBound(fileSize);
    uint8_t *payload = malloc(64 + compSize);
    if (!payload) {
        free(fileBuf);
        printf("Error 017: Memory allocation failed\n");
        return;
    }

    memset(payload, 0, 64);
    strncpy((char *)payload, saveName, 63);

    int res = compress(payload + 64, &compSize, fileBuf, fileSize);
    free(fileBuf);

    if (res != Z_OK) {
        printf("Error 003: Compression failed: %d\n", res);
        free(payload);
        return;
    }

    size_t totalPayloadSize = 64 + compSize;
    printf("Resync: Sending save to client (%zu bytes)...\n", totalPayloadSize);
    mg_ws_send(c, payload, totalPayloadSize, WEBSOCKET_OP_BINARY);
    free(payload);
    printf("Resync: Save sent successfully\n");
}

void eventManager(struct mg_connection *conn, int ev, void *evData){
    switch (ev)
    {
    case MG_EV_CONNECT: {
        struct mg_tls_opts opts = {0};
        opts.name = mg_str("shogun2-sync.duckdns.org");
        opts.skip_verification = true;
        mg_tls_init(conn, &opts);
        break;
    }
    case MG_EV_WS_OPEN:
        printf("Connected with lobby\n");
        if (shared != NULL) {
            if (isHost) {
                shared->status = SYNC_STATUS_CONNECTING;
                snprintf(shared->status_msg, sizeof(shared->status_msg), "Room created (%s). Waiting for partner...", currentRoomCode);
                strncpy(shared->room_code, currentRoomCode, sizeof(shared->room_code) - 1);
                shared->room_code[sizeof(shared->room_code) - 1] = '\0';
            } else {
                shared->status = SYNC_STATUS_CONNECTING;
                snprintf(shared->status_msg, sizeof(shared->status_msg), "Verifying room %s...", currentRoomCode);
                shared->room_code[0] = '\0';
            }
        }
        break;
    case MG_EV_WS_MSG: {
        struct mg_ws_message *wm = (struct mg_ws_message *) evData;
        int msgtype = wm->flags & 0x0F;
        if (msgtype == WEBSOCKET_OP_BINARY) {
            if (wm->data.len <= 64) {
                printf("Error 016: Invalid packet received\n");
                break;
            }

            char saveName[65];
            memcpy(saveName, wm->data.buf, 64);
            saveName[64] = '\0';

            printf("Resync: Received save from host: %s\n", saveName);

            char targetPath[512];
            snprintf(targetPath, sizeof(targetPath), "%s\\The Creative Assembly\\Shogun2\\save_games_multiplayer\\%s", G_APPDATA, saveName);

            uint8_t *compressedData = (uint8_t *)wm->data.buf + 64;
            size_t compressedLen = wm->data.len - 64;

            g_isRestoringSave = true;
            bool decErr = kw_decompress(compressedData, compressedLen, targetPath);
            if (decErr == 0) {
                patchMultiplayerSaveHeader(targetPath);
                printf("Resync: Save restored successfully\n");
                commit("Resync commit");
                g_lastRestoreTick = GetTickCount();
                if (lastHostTour >= 0) {
                    tour = lastHostTour;
                }
                strcpy(g_previousSaveHash, lastHostHash);
                strcpy(latestActiveHash, lastHostHash);
                strcpy(latestTree, lastHostHash);
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_OK;
                    shared->tour = tour;
                    strncpy(shared->lastSaveName, saveName, sizeof(shared->lastSaveName) - 1);
                    shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Save synced to disk. Synchronizing in-game reload...");
                }
                char readyPkg[128];
                snprintf(readyPkg, sizeof(readyPkg), "RELOAD_READY|%s", saveName);
                sendPackage(readyPkg, strlen(readyPkg));
            } else {
                printf("Error 019: Resync restore failed\n");
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DESYNC;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Resync restore failed");
                }
            }
            Sleep(500);
            g_isRestoringSave = false;
        } else if (msgtype == WEBSOCKET_OP_TEXT) {
            size_t textLen = wm->data.len + 1;
            char *msg = malloc(textLen * sizeof(char));
            memcpy(msg, wm->data.buf, wm->data.len);
            msg[textLen-1] = '\0';

            if (strncmp(msg, "REQ_STATUS", 10) == 0) {
                if (isHost) {
                    char statusPkg[256];
                    const char *sName = (shared != NULL && shared->lastSaveName[0] != '\0') ? shared->lastSaveName : "autosave.save_multiplayer";
                    snprintf(statusPkg, sizeof(statusPkg), "%s|%i|%s", latestTree, tour, sName);
                    sendPackage(statusPkg, strlen(statusPkg));
                }
            }
            else if (strncmp(msg, "REQ_SAVE|", 9) == 0) {
                char *requestedFile = msg + 9;
                printf("Resync requested for: %s\n", requestedFile);
                sendSaveToClient(requestedFile);
            }
            else if (strncmp(msg, "RELOAD_READY|", 13) == 0) {
                char *rSave = msg + 13;
                if (isHost) {
                    char execPkg[128];
                    snprintf(execPkg, sizeof(execPkg), "EXEC_RELOAD|%s", rSave);
                    sendPackage(execPkg, strlen(execPkg));
                    if (shared != NULL) {
                        strncpy(shared->lastSaveName, rSave, sizeof(shared->lastSaveName) - 1);
                        shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
                        shared->triggerReload = true;
                        snprintf(shared->status_msg, sizeof(shared->status_msg), "Synchronized dual-reload triggered.");
                    }
                }
            }
            else if (strncmp(msg, "EXEC_RELOAD|", 12) == 0) {
                char *rSave = msg + 12;
                if (!isHost) {
                    if (shared != NULL) {
                        strncpy(shared->lastSaveName, rSave, sizeof(shared->lastSaveName) - 1);
                        shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
                        shared->triggerReload = true;
                        snprintf(shared->status_msg, sizeof(shared->status_msg), "Synchronized dual-reload triggered.");
                    }
                }
            }
            else if (textLen > 41 && msg[40] == '|') {
                char receivedHash[41];
                memcpy(receivedHash, msg, 40 * sizeof(char));
                receivedHash[40] = '\0';

                int receivedTour = atoi(msg + 41);
                lastHostTour = receivedTour;
                memcpy(lastHostHash, receivedHash, sizeof(lastHostHash));

                const char *secondPipe = strchr(msg + 41, '|');
                if (secondPipe != NULL && shared != NULL) {
                    const char *saveFromHost = secondPipe + 1;
                    if (strlen(saveFromHost) > 0 && strstr(saveFromHost, "..") == NULL && strchr(saveFromHost, '/') == NULL && strchr(saveFromHost, '\\') == NULL) {
                        strncpy(shared->lastSaveName, saveFromHost, sizeof(shared->lastSaveName) - 1);
                        shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
                    }
                }

                if (!isHost) {
                    tour = receivedTour;
                    if (shared != NULL) {
                        shared->tour = tour;
                    }
                    char freshActiveName[MAX_PATH] = "";
                    getActiveCampaignSave(freshActiveName, sizeof(freshActiveName), latestActiveHash);
                    if (freshActiveName[0] != '\0' && shared != NULL) {
                        strncpy(shared->lastSaveName, freshActiveName, sizeof(shared->lastSaveName) - 1);
                        shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
                    }
                    if (shared != NULL) {
                        shared->status = SYNC_STATUS_OK;
                        snprintf(shared->status_msg, sizeof(shared->status_msg), "Synchronized (Tour %d)", tour);
                    }
                    printf("Turn %d synchronized with host\n", tour);
                }
            }
            else if (strstr(msg, "Room does not exist") != NULL || strstr(msg, "not exist") != NULL) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Room does not exist");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Room does not exist");
                printf("Error: Room does not exist\n");
                connection = false;
                running = false;
            }
            else if (strstr(msg, "Invalid room password") != NULL) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Invalid room password");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Invalid room password");
                printf("Error: Invalid room password\n");
                connection = false;
                running = false;
            }
            else if (strstr(msg, "Room is full") != NULL) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Room is full");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Room is full");
                printf("Error: Room is full\n");
                connection = false;
                running = false;
            }
            else if (strstr(msg, "already exists") != NULL) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Room already exists");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Room already exists");
                printf("Error: Room already exists\n");
                connection = false;
                running = false;
            }
            else {
                const char *p = strstr(msg, "Players in lobby: ");
                if (p != NULL) {
                    uint32_t users = 0, max_u = 0;
                    if (sscanf(p, "Players in lobby: %u/%u", &users, &max_u) == 2) {
                        if (shared != NULL) {
                            shared->lobbyUsers = users;
                            shared->lobbyMaxUsers = max_u;
                            if (currentRoomCode[0] != '\0') {
                                strncpy(shared->room_code, currentRoomCode, sizeof(shared->room_code) - 1);
                                shared->room_code[sizeof(shared->room_code) - 1] = '\0';
                            }
                        }
                        if (isHost) {
                            if (users == 1) {
                                if (shared != NULL) {
                                    shared->status = SYNC_STATUS_CONNECTING;
                                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Room created (%s). Waiting for partner... (1/%u)", currentRoomCode, max_u);
                                }
                            } else if (users >= 2) {
                                if (shared != NULL) {
                                    shared->status = SYNC_STATUS_OK;
                                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Lobby: %u/%u connected (Ready)", users, max_u);
                                }
                                char statusPkg[256];
                                const char *sName = (shared != NULL && shared->lastSaveName[0] != '\0') ? shared->lastSaveName : "autosave.save_multiplayer";
                                snprintf(statusPkg, sizeof(statusPkg), "%s|%i|%s", latestTree, tour, sName);
                                sendPackage(statusPkg, strlen(statusPkg));
                            }
                        } else {
                            if (users >= 2) {
                                if (shared != NULL) {
                                    shared->status = SYNC_STATUS_OK;
                                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Lobby: %u/%u connected (Ready)", users, max_u);
                                }
                                sendPackage("REQ_STATUS", 10);
                            }
                        }
                    }
                }
                printf("%s\n", msg);
                GuiLog(msg);
            }
            free(msg);
        }
        break;
    }
    case MG_EV_WS_CTL: {
        struct mg_ws_message *wm = (struct mg_ws_message *) evData;
        int msgtype = wm->flags & 0x0F;
        if (msgtype == WEBSOCKET_OP_CLOSE) {
            uint16_t closeCode = 0;
            if (wm->data.len >= 2) {
                closeCode = ((uint8_t)wm->data.buf[0] << 8) | (uint8_t)wm->data.buf[1];
            }
            if (closeCode == 4003) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Room does not exist");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Room does not exist");
                printf("Error: Room does not exist\n");
            } else if (closeCode == 4000) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Invalid room password");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Invalid room password");
                printf("Error: Invalid room password\n");
            } else if (closeCode == 4001) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Room is full");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Room is full");
                printf("Error: Room is full\n");
            } else if (closeCode == 4002) {
                if (shared != NULL) {
                    shared->status = SYNC_STATUS_DISCONNECTED;
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Room already exists");
                    shared->room_code[0] = '\0';
                    shared->lobbyUsers = 0;
                    shared->lobbyMaxUsers = 0;
                }
                currentRoomCode[0] = '\0';
                GuiLog("Error: Room already exists");
                printf("Error: Room already exists\n");
            }
            connection = false;
            running = false;
        }
        break;
    }
    case MG_EV_ERROR:
        printf("Error 015: Connection failed: %s\n", (char *) evData);
        if (shared != NULL && shared->status != SYNC_STATUS_DISCONNECTED) {
            shared->status = SYNC_STATUS_DISCONNECTED;
            snprintf(shared->status_msg, sizeof(shared->status_msg), "Connection error");
            shared->room_code[0] = '\0';
            shared->lobbyUsers = 0;
            shared->lobbyMaxUsers = 0;
        }
        currentRoomCode[0] = '\0';
        connection = false;
        running = false;
        break;
    case MG_EV_CLOSE:
        if (connection) {
            printf("Connection closed\n");
            if (shared != NULL && shared->status != SYNC_STATUS_DISCONNECTED) {
                shared->status = SYNC_STATUS_DISCONNECTED;
                snprintf(shared->status_msg, sizeof(shared->status_msg), "Connection closed");
                shared->room_code[0] = '\0';
                shared->lobbyUsers = 0;
                shared->lobbyMaxUsers = 0;
            }
            currentRoomCode[0] = '\0';
            connection = false;
            running = false;
        }
        break;
    }
}

void connectToRoom(char roomName[], char password[], char action[]){
    if (mg_initialized) {
        c = NULL;
        mg_mgr_free(&mg);
        mg_initialized = false;
    }
    mg_mgr_init(&mg);
    mg_initialized = true;
    mg_boot_timestamp_ms = (uint64_t)time(NULL) * 1000;
    lastHostTour = -1;
    memset(lastHostHash, 0, sizeof(lastHostHash));
    currentRoomCode[0] = '\0';
    size_t ri = 0;
    for (size_t i = 0; roomName[i] != '\0' && ri < sizeof(currentRoomCode) - 1; i++) {
        if (roomName[i] != ' ' && roomName[i] != '\t' && roomName[i] != '\r' && roomName[i] != '\n') {
            currentRoomCode[ri++] = (char)toupper((unsigned char)roomName[i]);
        }
    }
    currentRoomCode[ri] = '\0';
    char url[512];
    snprintf(url, sizeof(url), "wss://shogun2-sync.duckdns.org/ws/%s?password=%s&action=%s&max_users=2", currentRoomCode, password, action);
    c = mg_ws_connect(&mg, url, eventManager, NULL, NULL);
    if (c == NULL) {
        printf("Error 015: Connection failed\n");
        running = false;
    }
}

void disconnectFromRoom(void){
    connection = false;
    running = false;
    c = NULL;
    currentRoomCode[0] = '\0';
    if (mg_initialized) {
        mg_mgr_free(&mg);
        mg_initialized = false;
    }
}

void pollNetwork() {
    mg_mgr_poll(&mg, 50);
}

void sendPackage(char msg[], size_t leng){
    mg_ws_send(c, msg, leng, WEBSOCKET_OP_TEXT);
}