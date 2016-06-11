// Nintendont: SRAM functions.
#ifndef __SRAM_H__
#define __SRAM_H__

#include "global.h"

// GameCube SRAM data.
typedef struct _GC_SRAM
{
/* 0x00 */	u16 CheckSum1;
/* 0x02 */	u16 CheckSum2;
/* 0x04 */	u32 ead0;
/* 0x08 */	u32 ead1;
/* 0x0C */	u32 CounterBias;
/* 0x10 */	u8	DisplayOffsetH;
/* 0x11 */	u8	BootMode;	// Bit 6 PAL60 flag
/* 0x12 */	u8	Language;
/* 0x13 */	u8	Flags;
		/*
			bit			desc			0		1
			0			-\_ Video mode
			1			-/
			2			Sound mode		Mono	Stereo
			3			always 1
			4			always 0
			5			always 1
			6			?
			7			Prog mode		off		on
		*/
/* 0x14 */	u8	FlashID[2][12];
/* 0x2C */	u32	WirelessKBID;
/* 0x30 */	u16	WirlessPADID[4];
/* 0x38 */	u8	LastDVDError;
/* 0x39 */	u8	Reserved;
/* 0x3A */	u8	FlashIDChecksum[2];
/* 0x3E */	u32	Unused;
} GC_SRAM;

// Emulated SRAM instance.
extern GC_SRAM sram;

/**
 * Update the SRAM checksum.
 */
void SRAM_UpdateChecksum(void);

/**
 * Initialize SRAM.
 */
void SRAM_Init(void);

/**
 * Set the flash ID of a memory card in SRAM.
 * @param base Memory card base address.
 * @param slot Slot number. (0 or 1)
 */
void SRAM_SetFlashID(const u8 *base, int slot);

#endif /* __SRAM_H__ */
