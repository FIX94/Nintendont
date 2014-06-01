#ifndef __JVCIO__
#define __JVCIO__

#include "global.h"
#include "string.h"

void JVSIOMessage(void);
void JVSIOstart(int node);
void addDataBuffer(const void *data, size_t len);
void addDataString(const char *data);
void addDataByte(int n);
void end();
void addData(const unsigned char *dst, size_t len, int sync );

#endif
