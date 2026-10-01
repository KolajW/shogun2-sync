#include "kw_conn.h"
#include "kw_conf.h"


struct mg_connection *c = NULL;
struct mg_mgr mg;



void eventManager(struct mg_connection *conn, int ev, void *evData){
    switch (ev)
    {
    case MG_EV_WS_OPEN:
        printf("Connected with lobby\n");
        break;
    case MG_EV_WS_MSG:
        struct mg_ws_message *wm = (struct mg_ws_message *) evData;
        int msgtype = wm->flags & 0x0F;
        if (msgtype == WEBSOCKET_OP_BINARY) {
        // This is a binary data message
        } else if (msgtype == WEBSOCKET_OP_TEXT) {
            size_t textLen = wm->data.len + 1;
            char *msg = malloc(textLen * sizeof(char));
            memcpy(msg, wm->data.buf, wm->data.len);
            msg[textLen-1] = '\0';

            if (textLen > 41 && msg[40] == '|') {
                char receivedHash[41];
                memcpy(receivedHash,msg,40 * sizeof(char));
                receivedHash[40] = '\0';

                int receivedTour = atoi(msg + 41);

                if(receivedTour == tour){
                    int diff = strcmp(latest,receivedHash);
                    if(diff == 0){
                        printf("Saves are sync\n");
                    }
                    else
                    {
                        printf("Saves are desync\n");
                    }
                    
                }
            }
            else{
                printf("[SERWER] %s\n", msg);
            }
            free(msg);
        }
        break;
    }
}

void connectToRoom(char roomName[]){
    printf("Room: %s\nCreated successfully\n",roomName);
    mg_mgr_init(&mg);
    char url[256];
    snprintf(url, sizeof(url), "ws://192.168.1.61:8000/ws/%s", roomName);
    c = mg_ws_connect(&mg, url, eventManager, NULL, NULL);
}

void pollNetwork() {
    mg_mgr_poll(&mg, 50);
}

void sendPackage(char msg[],size_t leng){
    mg_ws_send(c,msg,leng,WEBSOCKET_OP_TEXT);
}