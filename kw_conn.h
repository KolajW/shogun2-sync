#include "mongoose/mongoose.h"

extern struct mg_connection *c;
extern struct mg_mgr mg;

struct mg_ws_message {
  struct mg_str data;
  uint8_t flags;
};

void eventManager(struct mg_connection *conn, int ev, void *evData);
void connectToRoom(char roomName[]);
void pollNetwork();
void sendPackage(char msg[],size_t leng);
