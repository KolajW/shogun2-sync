#include "kw_conn.h"
#include "kw_conf.h"
#include "zlib/zlib.h"

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
    case MG_EV_WS_OPEN:
        printf("Connected with lobby\n");
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

            bool decErr = kw_decompress(compressedData, compressedLen, targetPath);
            if (decErr == 0) {
                printf("Resync: Save restored successfully\n");
                commit("Resync commit");
            } else {
                printf("Error 019: Resync restore failed\n");
            }
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

                if (receivedTour == tour) {
                    int diff = strcmp(latest, receivedHash);
                    if (diff == 0) {
                        printf("Saves are sync\n");
                    }
                    else {
                        printf("Saves are desync\n");
                        printf("Resync: Requesting save from host...\n");
                        char req[64];
                        snprintf(req, sizeof(req), "REQ_SAVE|autosave.save_multiplayer");
                        sendPackage(req, strlen(req));
                    }
                }
            }
            else {
                printf("%s\n", msg);
            }
            free(msg);
        }
        break;
    }
    case MG_EV_ERROR:
        printf("Error 015: Connection failed: %s\n", (char *) evData);
        connection = false;
        running = false;
        break;
    case MG_EV_CLOSE:
        if (connection) {
            printf("Connection closed\n");
            connection = false;
            running = false;
        }
        break;
    }
}

void connectToRoom(char roomName[], char password[], char action[]){
    mg_mgr_init(&mg);
    char url[512];
    snprintf(url, sizeof(url), "wss://shogun2-sync.duckdns.org/ws/%s?password=%s&action=%s", roomName, password, action);
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