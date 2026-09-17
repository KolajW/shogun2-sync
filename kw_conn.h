#include "mongoose/mongoose.h"

extern struct mg_connection *c;

extern struct mg_mgr mg;

void eventManager(struct mg_connection *conn, int ev, void *evData);
void connectToRoom(char roomName[]);
void pollNetwork();
