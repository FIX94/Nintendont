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

void PatchB( u32 dst, u32 src );
void PatchBL( u32 dst, u32 src );
u32 PatchCopy(const u8 *PatchPtr, const u32 PatchSize);
s32 PatchFunc( char *ptr );
void PatchFuncInterface( char *dst, u32 Length );
void PatchPatchBuffer(char *dst);
void CheckPatchPrs();
s32 Check_Cheats();

void DoCardPatches( char *ptr, u32 size );
void DoPatches( char *Buffer, u32 Length, u32 Offset );
void SetIPL();
void SetIPL_TRI();

void PatchInit();
void PatchGame();

void MPattern( u8 *Data, u32 Length, FuncPattern *FunctionPattern );
int CPattern( FuncPattern *FPatA, FuncPattern *FPatB  );

#endif
