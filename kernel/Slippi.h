#ifndef __SLIPPI_H__
#define __SLIPPI_H__

#include "global.h"

void SlippiInit();
void SlippiShutdown();

void SlippiImmWrite(u32 data, u32 size);
void SlippiDmaWrite(const void *buf, u32 len);

u32 SlippiHandlerThread(void *arg);

#endif
