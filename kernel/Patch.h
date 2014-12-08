#ifndef __PATCH_H__
#define __PATCH_H__

#include "global.h"

#define VI_NTSC				0
#define VI_PAL				1
#define VI_MPAL				2
#define VI_DEBUG			3
#define VI_DEBUG_PAL		4
#define VI_EUR60			5
#define VI_480P				6

#define GXPal528IntDf		0
#define GXEurgb60Hz480IntDf	1
#define GXMpal480IntDf		2
#define GXNtsc480IntDf		3
#define GXNtsc480Int		4
#define GXNtsc480Prog		5
#define GXPal528Prog		6
#define GXEurgb60Hz480Prog	7

typedef struct FuncPattern
{
	u32 Length;
	u32 Loads;
	u32 Stores;
	u32 FCalls;
	u32 Branch;
	u32 Moves;
	const void *Patch;
	u32 PatchLength;
	const char *Name;
	const char *Type;
	u32 Group;
	u32 Found;
} FuncPattern;

typedef struct FuncPatterns
{
	FuncPattern *pat;
	u32 patlen;
	u32 patmode;
} FuncPatterns;

typedef struct GC_SRAM 
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
/* 0x3E */	u16	Unused;
} GC_SRAM;


void PatchB( u32 dst, u32 src );
void PatchBL( u32 dst, u32 src );
s32 PatchFunc( char *ptr );
void PatchFuncInterface( char *dst, u32 Length );
void PatchPatchBuffer(char *dst);
void CheckPatchPrs();
s32 Check_Cheats();

void DoCardPatches( char *ptr, u32 size );
void DoPatches( char *Buffer, u32 Length, u32 Offset );

void PatchInit();
void PatchGame();

void MPattern( u8 *Data, u32 Length, FuncPattern *FunctionPattern );
int CPattern( FuncPattern *FPatA, FuncPattern *FPatB  );

void SRAM_Checksum( unsigned short *buf, unsigned short *c1, unsigned short *c2);

#endif
