#ifndef __EXI_H__
#define __EXI_H__
#include "global.h"

#ifdef EXIPATCH

enum
{
	BBA_RECV_SIZE	= 0x800,
	BBA_MEM_SIZE	= 0x1000,

	CB_OFFSET	= 0x100,
	CB_SIZE		= (BBA_MEM_SIZE - CB_OFFSET),
	SIZEOF_ETH_HEADER		= 0xe,
	SIZEOF_RECV_DESCRIPTOR	= 4,
	
	EXI_DEVTYPE_MODEM	    = 0x02020000,
	EXI_DEVTYPE_ETHER	    = 0x04020200,
	EXI_DEVTYPE_AD16	    = 0x04120000,
	EXI_DEVTYPE_BASEBOARD	= 0x06041000,
};

#define IPL_ROM_FONT_SJIS				0x1AFF00
#define IPL_ROM_FONT_ANSI				0x1FCF00
#define IPL_ROM_END_OFFSET				0x200000

#define MC_STATUS_BUSY					0x80
#define MC_STATUS_UNLOCKED			0x40
#define MC_STATUS_SLEEP					0x20
#define MC_STATUS_ERASEERROR		0x10
#define MC_STATUS_PROGRAMEERROR	0x08
#define MC_STATUS_READY					0x01
#define SIZE_TO_Mb							(1024 * 8 * 16)
#define MC_HDR_SIZE							0xA000

#define		EXI_BASE	0x13026800

#define		EXI_CMD_0	(EXI_BASE+0x00)
#define		EXI_CMD_1	(EXI_BASE+0x04)

#define EXI_READ			0
#define EXI_WRITE			1
#define EXI_READWRITE	2 

void EXIInit();

/**
 * Set EXI timings based on the game ID.
 * @param TitleID Game ID, rshifted by 8.
 * @param Region Region byte from Game ID.
 */
void EXISetTimings(u32 TitleID, u32 Region);

bool EXICheckTimer();
void EXIInterrupt();

/**
 * Get the total size of the loaded memory cards.
 * @return Total size, in bytes.
 */
u32 EXIGetTotalCardSize(void);

/**
 * Check if any memory cards have changed.
 * @return True if either memory card has changed; false if not.
 */
bool EXICheckCard(void);

/**
 * Save the memory card(s).
 */
void EXISaveCard(void);

void EXIShutdown(void);
void EXIUpdateRegistersNEW( void );
void EXIReadFontFile(u8* Data, u32 Length);
void EXIPrepareTRIGameStart();

#endif

#endif
