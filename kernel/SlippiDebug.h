#ifndef __SLIPPI_DEBUG_H__
#define __SLIPPI_DEBUG_H__

#include "string.h"

// Function declarations
int vsCheckCSS(void);
int checkCrash(int socket);

/* ppc_msg()
 * Copy some buffer into the MEM1 region (starting at 0x8040a5a8) read by
 * UnclePunch's patch. In order for this to work, you need to have a GALE01 
 * GCT prepared with the relevant Gecko codes in order for this to work. This 
 * is a reasonable alternative for getting some feedback about the state of 
 * ARM-world during runtime in cases where you don't have a USB Gecko.
 */

// Pointer to unused .data region (HIO/EXI2USB functionality)
#define UP_PPC_BUFFER	(void*)0x0040a5a8
#define ppc_msg(buf, size)					\
	memcpy(UP_PPC_BUFFER, buf, size);			\
	sync_after_write(UP_PPC_BUFFER, (size + 0x20));		\


/* ----------------------------------------------------------------------------
 * Useful physical addresses to keep around when debugging Melee from Starlet.
 */

// This should increment every frame (starting at early boot-time)
#define MELEE_FRAME_CTR		0x004d7420

// This is the top (meaning, lowest address) of the stack region in NTSC 1.02
#define MELEE_STACK_TOP		0x004dec00
#define MELEE_STACK_SIZE	0x00010000

// Current scene data. Major scene is the first byte. Minor scene is the last byte.
#define MELEE_CURRENT_SCENE	0x00479d30
#define MELEE_MAJ_SCENE_TABLE	0x003daca4

#endif //__SLIPPI_DEBUG_H__
