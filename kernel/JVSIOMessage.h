#ifndef __JVCIO__
#define __JVCIO__

#include "global.h"
#include "string.h"

void JVSIOMessage(void);
void JVSIOstart(int node);
void addDataBuffer(const void *data, size_t len);
void addDataString(const char *data);
void addDataByte(const u8 data);
void end();
void addData(const void *data, size_t len, int sync);

#endif
