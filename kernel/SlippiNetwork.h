#ifndef __SLIPPI_NETWORK_H__
#define __SLIPPI_NETWORK_H__

#include "global.h"

// Server state
#define CONN_STATUS_UNKNOWN 0
#define CONN_STATUS_NO_SERVER 1
#define CONN_STATUS_NO_CLIENT 2
#define CONN_STATUS_CONNECTED 3 

#define MAX_TX_SIZE 2048

s32 SlippiNetworkInit();
void SlippiNetworkShutdown();
void SlippiNetworkSetNewFile(const char* path);

int getConnectionStatus();

#endif
