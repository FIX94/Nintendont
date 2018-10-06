#ifndef __SLIPPI_DEBUG_H__
#define __SLIPPI_DEBUG_H__

#include "string.h"

#define UP_DEBUG	1
#define UP_PPC_BUFFER	(void*)0x0040a5a8

/* Copy some buffer into the MEM1 region (starting at 0x8040a5a8) read by 
 * UnclePunch's patch. In order for this to work, you need to have a GALE01 
 * GCT prepared with the relevant Gecko codes in order for this to work. This 
 * is a reasonable alternative for getting some feedback about the state of 
 * ARM-world during runtime in cases where you don't have a USB Gecko.
 */
#define ppc_msg(buf, size)					\
	memcpy(UP_PPC_BUFFER, buf, size);			\
	sync_after_write(UP_PPC_BUFFER, (size + 0x20));		\

#endif //__SLIPPI_DEBUG_H__
