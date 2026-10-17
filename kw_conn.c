#include "kw_conn.h"
#include "kw_conf.h"
#include "kw_gui.h"
#include "zlib/zlib.h"
#include <time.h>

struct mg_connection *c = NULL;
struct mg_mgr mg;
int lastHostTour = -1;
char lastHostHash[41] = {0};

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
            shared->status = SYNC_STATUS_OK;
            snprintf(shared->status_msg, sizeof(shared->status_msg), "Connected with lobby");
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
                    shared->triggerReload = true;
                    strncpy(shared->lastSaveName, saveName, sizeof(shared->lastSaveName) - 1);
                    shared->lastSaveName[sizeof(shared->lastSaveName) - 1] = '\0';
                    snprintf(shared->status_msg, sizeof(shared->status_msg), "Save injected - reloading...");
                }
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

            if (strncmp(msg, "REQ_SAVE|", 9) == 0) {
                char *requestedFile = msg + 9;
                printf("Resync requested for: %s\n", requestedFile);
                sendSaveToClient(requestedFile);
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
                    if (receivedTour == tour) {
                        int diff = strcmp(latestActiveHash, receivedHash);
                        if (diff == 0) {
                            printf("Saves are sync\n");
                            if (shared != NULL) {
                                shared->status = SYNC_STATUS_OK;
                                snprintf(shared->status_msg, sizeof(shared->status_msg), "Synchronized (Tour %d)", tour);
                            }
                        }
                        else {
                            printf("Saves are desync\n");
                            if (shared != NULL) {
                                shared->status = SYNC_STATUS_DESYNC;
                                snprintf(shared->status_msg, sizeof(shared->status_msg), "Desync detected! Resyncing...");
                            }
                            printf("Resync: Requesting save from host...\n");
                            char req[128];
                            const char *tgt = (shared != NULL && shared->lastSaveName[0] != '\0') ? shared->lastSaveName : "autosave.save_multiplayer";
                            snprintf(req, sizeof(req), "REQ_SAVE|%s", tgt);
                            sendPackage(req, strlen(req));
                        }
                    } else if (receivedTour > tour) {
                        if (shared != NULL) {
                            shared->status = SYNC_STATUS_OK;
                            snprintf(shared->status_msg, sizeof(shared->status_msg), "Host at Tour %d - waiting for turn...", receivedTour);
                        }
                    }
                }
            }
            else {
                const char *p = strstr(msg, "Players in lobby: ");
                if (p != NULL) {
                    uint32_t users = 0, max_u = 0;
                    if (sscanf(p, "Players in lobby: %u/%u", &users, &max_u) == 2) {
                        if (shared != NULL) {
                            shared->lobbyUsers = users;
                            shared->lobbyMaxUsers = max_u;
                            snprintf(shared->status_msg, sizeof(shared->status_msg), "Lobby: %u/%u connected", users, max_u);
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
    case MG_EV_ERROR:
        printf("Error 015: Connection failed: %s\n", (char *) evData);
        if (shared != NULL) {
            shared->status = SYNC_STATUS_DISCONNECTED;
            snprintf(shared->status_msg, sizeof(shared->status_msg), "Connection error");
        }
        connection = false;
        running = false;
        break;
    case MG_EV_CLOSE:
        if (connection) {
            printf("Connection closed\n");
            if (shared != NULL) {
                shared->status = SYNC_STATUS_DISCONNECTED;
                snprintf(shared->status_msg, sizeof(shared->status_msg), "Connection closed");
            }
            connection = false;
            running = false;
        }
        break;
    }
}

void connectToRoom(char roomName[], char password[], char action[]){
    mg_mgr_init(&mg);
    mg_boot_timestamp_ms = (uint64_t)time(NULL) * 1000;
    char url[512];
    snprintf(url, sizeof(url), "wss://shogun2-sync.duckdns.org/ws/%s?password=%s&action=%s&max_users=2", roomName, password, action);
    c = mg_ws_connect(&mg, url, eventManager, NULL, NULL);
    if (c == NULL) {
        printf("Error 015: Connection failed\n");
        running = false;
    }
}

void pollNetwork() {
    mg_mgr_poll(&mg, 50);
}

void sendPackage(char msg[], size_t leng){
    mg_ws_send(c, msg, leng, WEBSOCKET_OP_TEXT);
}