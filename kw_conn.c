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
        
        break;
    }
}

void connectToRoom(char roomName[]){
    mg_mgr_init(&mg);
    c = mg_ws_connect(&mg, "ws://localhost:8000/ws/SHOGUN", eventManager, NULL, NULL);
}

void pollNetwork() {
    mg_mgr_poll(&mg, 10);
}

void sendPackage(char msg[],size_t leng){
    mg_ws_send(c,msg,leng,WEBSOCKET_OP_TEXT);
}