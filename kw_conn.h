#ifndef KW_CONN_H
#define KW_CONN_H

#ifndef MG_TLS
#define MG_TLS MG_TLS_BUILTIN
#endif
#include "mongoose/mongoose.h"

extern struct mg_connection *c;
extern struct mg_mgr mg;

void eventManager(struct mg_connection *conn, int ev, void *evData);
void connectToRoom(char roomName[], char password[], char action[]);
void pollNetwork();
void sendPackage(char msg[], size_t leng);
void sendSaveToClient(const char *saveName);

#endif