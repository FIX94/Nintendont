/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar
Copyright (C) 2014 - 2019 FIX94

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "Patch.h"
#include "string.h"
#include "dol.h"
#include "elf.h"
#include "PatchCodes.h"
#include "PatchWidescreen.h"
#include "PatchTimers.h"
#include "TRI.h"
#include "Config.h"
#include "global.h"
#include "patches.c"
#include "DI.h"
#include "ISO.h"
#include "SI.h"
#include "EXI.h"
#include "sock.h"
#include "codehandler.h"
#include "codehandleronly.h"
#include "ff_utf8.h"

//#define DEBUG_DSP  // Very slow!! Replace with raw dumps?

u32 GAME_ID	= 0;
u16 GAME_ID6 = 0;
u32 TITLE_ID = 0;

#define PATCH_OFFSET_START (0x3000 - (sizeof(u32) * 3))
#define PATCH_OFFSET_ENTRY PATCH_OFFSET_START - FakeEntryLoad_size
static u32 POffset = PATCH_OFFSET_ENTRY;
vu32 useipl = 0, useipltri = 0;
vu32 DisableSIPatch = 0, DisableEXIPatch = 0, bbaEmuWanted = 0;
extern vu32 TRIGame;
extern vu16 SOCurrentTotalFDs;

#define PRS_DOL     0x131C0000
#define PRS_EXTRACT 0x131C0020

#define PATCH_STATE_NONE  0
#define PATCH_STATE_LOAD  1
#define PATCH_STATE_PATCH 2
#define PATCH_STATE_DONE  4

#define PSO_STATE_NONE          0
#define PSO_STATE_LOAD          1
#define PSO_STATE_PSR           2
#define PSO_STATE_NOENTRY       4
#define PSO_STATE_SWITCH        8
#define PSO_STATE_SWITCH_START  16

#define SONICRIDERS_BASE_NTSCU	0x4FBD00
#define SONICRIDERS_BASE_NTSCJ	0x4FBD00
#define SONICRIDERS_BASE_PAL	0x4FBDC0

#define SONICRIDERS_HOOK_NTSCU	0x555214
#define SONICRIDERS_HOOK_NTSCJ	0x5551A8
#define SONICRIDERS_HOOK_PAL	0x5554E8

u32 PatchState = PATCH_STATE_NONE;
u32 PSOHack    = PSO_STATE_NONE;
u32 ELFLoading = 0;
u32 DOLSize    = 0;
u32 DOLMinOff  = 0;
u32 DOLMaxOff  = 0;
vu32 TRI_BackupAvailable = 0;
vu32 GameEntry = 0, FirstLine = 0;
u32 AppLoaderSize = 0;
u32 MAT2patched = 0;
u32 NeedRelPatches = 0;
u32 NeedConstantRelPatches = 0;

static char cheatPath[255];
extern u32 prs_decompress(void* source,void* dest);
extern u32 prs_decompress_size(void* source);

static FuncPattern curFunc;

#ifndef DEBUG_PATCH
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

extern u32 UseReadLimit;
extern u32 RealDiscCMD;
extern u32 drcAddress;
extern u32 drcAddressAligned;
u32 IsN64Emu = 0;
u32 isKirby = 0;
u32 isdisneyskt = 0;

// SHA-1 hashes of known DSP modules.
static const unsigned char DSPHashes[][20] =
{
	// 0: Dolphin = 0x86840740 = Zelda:WW
	{0xC9, 0x7D, 0x1E, 0xD0, 0x71, 0x90, 0x47, 0x3F, 0x6A, 0x66, 0x42, 0xB2, 0x7E, 0x4A, 0xDB, 0xCD, 0xB6, 0xF8, 0x8E, 0xC3},

	// 1: Dolphin = 0x56d36052 = Super Mario Sunshine
	{0x21, 0xD0, 0xC0, 0xEE, 0x25, 0x3D, 0x8C, 0x9E, 0x02, 0x58, 0x66, 0x7F, 0x3C, 0x1B, 0x11, 0xBC, 0x90, 0x1F, 0x33, 0xE2},

	// 2: Dolphin = 0x4e8a8b21 = Sonic Mega Collection
	{0xBA, 0xDC, 0x60, 0x15, 0x33, 0x33, 0x28, 0xED, 0xB1, 0x0E, 0x72, 0xF2, 0x5B, 0x5A, 0xFB, 0xF3, 0xEF, 0x90, 0x30, 0x90},

	// 3: Dolphin = 0xe2136399 = Mario Party 5
	{0xBC, 0x17, 0x36, 0x81, 0x7A, 0x14, 0xB2, 0x1C, 0xCB, 0xF7, 0x3A, 0xD6, 0x8F, 0xDA, 0x57, 0xF8, 0x43, 0x78, 0x1A, 0xAE},

	// 4: Dolphin = 0x07f88145 = Zelda:OOT
	{0xD9, 0x39, 0x63, 0xE3, 0x91, 0xD1, 0xA8, 0x5E, 0x4D, 0x5F, 0xD9, 0xC2, 0x9A, 0xF9, 0x3A, 0xA9, 0xAF, 0x8D, 0x4E, 0xF2},

	// 5: Dolphin = 0x2fcdf1ec = Mario Kart DD
	{0xD8, 0x12, 0xAC, 0x09, 0xDC, 0x24, 0x50, 0x6B, 0x0D, 0x73, 0x3B, 0xF5, 0x39, 0x45, 0x1A, 0x23, 0x85, 0xF3, 0xC8, 0x79},

	// 6: Dolphin = 0x3daf59b9 = Star Fox Assault
	{0x37, 0x44, 0xC3, 0x82, 0xC8, 0x98, 0x42, 0xD4, 0x9D, 0x68, 0x83, 0x1C, 0x2B, 0x06, 0x7E, 0xC7, 0xE8, 0x64, 0x32, 0x44},

	// 7: 0-Length DSP - Error
	{0xDA, 0x39, 0xA3, 0xEE, 0x5E, 0x6B, 0x4B, 0x0D, 0x32, 0x55, 0xBF, 0xEF, 0x95, 0x60, 0x18, 0x90, 0xAF, 0xD8, 0x07, 0x09},

	// 8: Dolphin = 0x6ca33a6d = Donkey Kong Jungle Beat, Zelda:TP
	{0x8E, 0x5C, 0xCA, 0xEA, 0xA9, 0x84, 0x87, 0x02, 0xFB, 0x5C, 0x19, 0xD4, 0x18, 0x6E, 0xA7, 0x7B, 0xE5, 0xB8, 0x71, 0x78},

	// 9: Dolphin = 0x3ad3b7ac = Paper Mario The Thousand Year Door
	{0xBC, 0x1E, 0x0A, 0x75, 0x09, 0xA6, 0x3E, 0x7C, 0xE6, 0x30, 0x44, 0xBE, 0xCC, 0x8D, 0x67, 0x1F, 0xA7, 0xC7, 0x44, 0xE5},

	// 10: Dolphin = 0x4be6a5cb = Animal Crossing, Pikmin 1 (USA)
	{0x14, 0x93, 0x40, 0x30, 0x0D, 0x93, 0x24, 0xE3, 0xD3, 0xFE, 0x86, 0xA5, 0x68, 0x2F, 0x4C, 0x13, 0x38, 0x61, 0x31, 0x6C},

	// 11: Dolphin = 0x42f64ac4 = Luigi's Mansion
	{0x09, 0xF1, 0x6B, 0x48, 0x57, 0x15, 0xEB, 0x3F, 0x67, 0x3E, 0x19, 0xEF, 0x7A, 0xCF, 0xE3, 0x60, 0x7D, 0x2E, 0x4F, 0x02},

	// 12: Dolphin = 0x267fd05a = Pikmin 1 (PAL)
	{0x80, 0x01, 0x60, 0xDF, 0x89, 0x01, 0x9E, 0xE3, 0xE8, 0xF7, 0x47, 0x2C, 0xE0, 0x1F, 0xF6, 0x80, 0xE9, 0x85, 0xB0, 0x24},	

	// 13: Dolphin = 0x6ba3b3ea = IPL NTSC 1.2 [NOTE: Listed as PAL IPL in Dolphin.]
	{0xB4, 0xCB, 0xC0, 0x0F, 0x51, 0x2C, 0xFE, 0xE5, 0xA4, 0xBA, 0x2A, 0x59, 0x60, 0x8A, 0xEB, 0x8C, 0x86, 0xC4, 0x61, 0x45},

	// 14: Dolphin = 0x24b22038 = IPL NTSC 1.0
	{0xA5, 0x13, 0x45, 0x90, 0x18, 0x30, 0x00, 0xB1, 0x34, 0x44, 0xAE, 0xDB, 0x61, 0xC5, 0x12, 0x0A, 0x72, 0x66, 0x07, 0xAA},

	// 15: Dolphin = 0x3389a79e = Metroid Prime Trilogy Wii (Needed?)
	{0x9F, 0x3C, 0x9F, 0x9E, 0x05, 0xC7, 0xD5, 0x0B, 0x38, 0x49, 0x2F, 0x2C, 0x68, 0x75, 0x30, 0xFD, 0xE8, 0x6F, 0x9B, 0xCA},

	// 16: Dolphin = 0xdf059f68 = Pikmin US Demo (Unknown to Dolphin)
	{0x13, 0x07, 0xEF, 0xAF, 0x7E, 0x66, 0x96, 0x60, 0xCE, 0x00, 0x2B, 0x89, 0x6E, 0x7E, 0xD4, 0xE7, 0xD8, 0x0F, 0x11, 0xF7},
};

// DSP patterns.
static const unsigned char DSPPattern[][0x10] =
{
	// 0: Hash 12, 1, 0, 5, 8
	{0x02, 0x9F, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00},

	// 1: Hash 14, 13, 11, 10, 16
	{0x02, 0x9F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00},

	// 2: Hash 2
	{0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0C, 0x10, 0x02, 0x9F, 0x0C, 0x1F, 0x02, 0x9F, 0x0C, 0x3B},

	// 3: Hash 3
	{0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x76, 0x02, 0x9F, 0x0E, 0x85, 0x02, 0x9F, 0x0E, 0xA1},

	// 4: Hash 4
	{0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0xB3, 0x02, 0x9F, 0x0E, 0xC2, 0x02, 0x9F, 0x0E, 0xDE},

	// 5: Hash 9
	{0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x71, 0x02, 0x9F, 0x0E, 0x80, 0x02, 0x9F, 0x0E, 0x9C},

	// 6: Hash 6
	{0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x88, 0x02, 0x9F, 0x0E, 0x97, 0x02, 0x9F, 0x0E, 0xB3},

	// 7: Hash 15
	{0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0D, 0xB0, 0x02, 0x9F, 0x0D, 0xBF, 0x02, 0x9F, 0x0D, 0xDB},
};

typedef struct DspMatch
{
	u32 Length;
	u32 Pattern;
	u32 SHA1;
} DspMatch;

static const DspMatch DspMatches[] =
{
	// Order Patterns together by increasing Length
	// Length, Pattern,   SHA1
	{ 0x00001A60,    0,     12 },
	{ 0x00001CE0,    0,      1 },
	{ 0x00001D20,    0,      0 },
	{ 0x00001D20,    0,      5 },
	{ 0x00001F00,    0,      8 },
	{ 0x00001280,    1,     14 },
	{ 0x00001760,    1,     13 },
	{ 0x000017E0,    1,     11 },
	{ 0x00001A00,    1,     10 },
	{ 0x00001B80,    1,     16 },
	{ 0x000019E0,    2,      2 },
	{ 0x00001EC0,    3,      3 },
	{ 0x00001F20,    4,      4 },
	{ 0x00001EC0,    5,      9 },
	{ 0x00001F00,    6,      6 },
	{ 0x00001D40,    7,     15 },
};

#define AX_DSP_NO_DUP3 (0xFFFF)
static void PatchAX_Dsp(u32 ptr, u32 Dup1, u32 Dup2, u32 Dup3, u32 Dup2Offset)
{
	static const u32 MoveLength = 0x22;
	static const u32 CopyLength = 0x12;
	static const u32 CallLength = 0x2 + 0x2; // call (2) ; jmp (2)
	static const u32 New1Length = 0x1 * 3 + 0x2 + 0x7; // 3 * orc (1) ; jmp (2) ; patch (7)
	static const u32 New2Length = 0x1; // ret
	u32 SourceAddr = Dup1 + MoveLength;
	u32 DestAddr = Dup2 + CallLength + CopyLength + New2Length;
	u32 Dup2PAddr = DestAddr; // Dup2+0x17 (0xB Patch fits before +0x22)
	u32 Tmp;

	DestAddr--;
	W16((u32)ptr + (DestAddr)* 2, 0x02DF);  // ret
	while (SourceAddr >= Dup1 + 1)  // ensure include Dup1 below
	{
		SourceAddr -= 2;
		Tmp = R32((u32)ptr + (SourceAddr)* 2); // original instructions
		if ((Tmp & 0xFFFFFFFF) == 0x195E2ED1) // lrri        $AC0.M srs         @SampleFormat, $AC0.M
		{
			DestAddr -= 7;
			W32((u32)ptr + (DestAddr + 0x0) * 2, 0x03400003); // andi        $AC1.M, #0x0003
			W32((u32)ptr + (DestAddr + 0x2) * 2, 0x8100009E); // clr         $ACC0 -
			W32((u32)ptr + (DestAddr + 0x4) * 2, 0x200002CA); // lri         $AC0.M, 0x2000 lsrn
			W16((u32)ptr + (DestAddr + 0x6) * 2, 0x1FFE);     // mrr     $AC1.M, $AC0.M
			Tmp = Tmp | 0x00010100; // lrri        $AC1.M srs         @SampleFormat, $AC1.M
		}
		DestAddr -= 2;
		W32((u32)ptr + (DestAddr)* 2, Tmp); // unchanged
		if ((Tmp & 0x0000FFF1) == 0x00002ED0) // srs @ACxAH, $AC0.M
		{
			DestAddr -= 1;
			W32((u32)ptr + (DestAddr)* 2, (Tmp & 0xFFFF0000) | 0x3E00); //  orc AC0.M AC1.M
		}
		if (DestAddr == Dup2 + CallLength) // CopyLength must be even
		{
			DestAddr = Dup1 + CallLength + MoveLength - CopyLength + New1Length;
			DestAddr -= 2;
			W32((u32)ptr + (DestAddr)* 2, 0x029F0000 | (Dup2 + CallLength)); // jmp Dup2+4
		}
	}
	W32((u32)ptr + (Dup1 + 0x00) * 2, 0x02BF0000 | (Dup1 + CallLength)); // call Dup1+4
	W32((u32)ptr + (Dup1 + 0x02) * 2, 0x029F0000 | (Dup1 + MoveLength)); // jmp Dup1+0x22
	W32((u32)ptr + (Dup2 + 0x00) * 2, 0x02BF0000 | (Dup1 + CallLength)); // call Dup1+4
	W32((u32)ptr + (Dup2 + 0x02) * 2, 0x029F0000 | (Dup2 + MoveLength)); // jmp Dup2+0x22
	Tmp = R32((u32)ptr + (Dup1 + 0x98) * 2); // original instructions
	W32((u32)ptr + (Dup1 + 0x98) * 2, 0x02BF0000 | (Dup2PAddr)); // call Dup2PAddr
	W32((u32)ptr + (Dup2 + Dup2Offset) * 2, 0x02BF0000 | (Dup2PAddr)); // call Dup2PAddr

	W16((u32)ptr + (Dup2PAddr + 0x00) * 2, Tmp >> 16); //  original instructions (Start of Dup2PAddr [0xB long])
	W32((u32)ptr + (Dup2PAddr + 0x01) * 2, 0x27D10340); // lrs         $AC1.M, @SampleFormat -
	W32((u32)ptr + (Dup2PAddr + 0x03) * 2, 0x00038100); // andi        $AC1.M, #0x0003 clr         $ACC0
	W32((u32)ptr + (Dup2PAddr + 0x05) * 2, 0x009E1FFF); // lri         $AC0.M, #0x1FFF
	W16((u32)ptr + (Dup2PAddr + 0x07) * 2, 0x02CA);     // lsrn
	W16((u32)ptr + (Dup2PAddr + 0x08) * 2, Tmp & 0xFFFF); //  original instructions
	W32((u32)ptr + (Dup2PAddr + 0x09) * 2, 0x3D0002DF); // andc        $AC1.M, $AC0.M ret

	if (Dup3 != AX_DSP_NO_DUP3)
	{
		W32((u32)ptr + (Dup3 + 0x00) * 2, 0x02BF0000 | (Dup1 + CallLength)); // call Dup1+4
		W32((u32)ptr + (Dup3 + 0x02) * 2, 0x029F0000 | (Dup3 + MoveLength)); // jmp Dup3+0x22

		W32((u32)ptr + (Dup3 + 0x04) * 2, 0x27D10340); // lrs         $AC1.M, @SampleFormat -
		W32((u32)ptr + (Dup3 + 0x06) * 2, 0x00038100); // andi        $AC1.M, #0x0003 clr         $ACC0
		W32((u32)ptr + (Dup3 + 0x08) * 2, 0x009E1FFF); // lri         $AC0.M, #0x1FFF
		W16((u32)ptr + (Dup3 + 0x0A) * 2, 0x02CA);     // lsrn
		Tmp = R32((u32)ptr + (Dup3 + 0x5D) * 2); // original instructions
		W16((u32)ptr + (Dup3 + 0x0B) * 2, Tmp >> 16); //  original instructions
		W16((u32)ptr + (Dup3 + 0x0C) * 2, 0x3D00); // andc        $AC1.M, $AC0.M
		W16((u32)ptr + (Dup3 + 0x0D) * 2, Tmp & 0xFFFF); //  original instructions
		Tmp = R32((u32)ptr + (Dup3 + 0x5F) * 2); // original instructions
		W32((u32)ptr + (Dup3 + 0x0E) * 2, Tmp); //  original instructions includes ret

		W32((u32)ptr + (Dup3 + 0x5D) * 2, 0x029F0000 | (Dup3 + CallLength)); // jmp Dup3+0x4
	}
	return;
}

static void PatchZelda_Dsp(u32 DspAddress, u32 PatchAddr, u32 OrigAddress, bool Split, bool KeepOrig)
{
	u32 Tmp = R32(DspAddress + (OrigAddress + 0) * 2); // original instructions at OrigAddress
	if (Split)
	{
		W32(DspAddress + (PatchAddr + 0) * 2, (Tmp & 0xFFFF0000) | 0x00000260); // split original instructions at OrigAddress
		W32(DspAddress + (PatchAddr + 2) * 2, 0x10000000 | (Tmp & 0x0000FFFF)); // ori $AC0.M, 0x1000 in between
	}
	else
	{
		W32(DspAddress + (PatchAddr + 0) * 2, 0x02601000); // ori $AC0.M, 0x1000
		W32(DspAddress + (PatchAddr + 2) * 2, Tmp);        // original instructions at OrigAddress
	}
	if (KeepOrig)
	{
		Tmp = R32(DspAddress + (PatchAddr + 4) * 2);       // original instructions at end of patch
		Tmp = (Tmp & 0x0000FFFF) | 0x02DF0000;             // ret/
	}
	else
		Tmp = 0x02DF02DF;                                  // ret/ret
	W32(DspAddress + (PatchAddr + 4) * 2, Tmp);
	W32(DspAddress + (OrigAddress + 0) * 2, 0x02BF0000 | PatchAddr);  // call PatchAddr
}
static void PatchZeldaInPlace_Dsp(u32 DspAddress, u32 OrigAddress)
{
	W32(DspAddress + (OrigAddress + 0) * 2, 0x009AFFFF); // lri $AX0.H, #0xffff
	W32(DspAddress + (OrigAddress + 2) * 2, 0x2AD62AD7); // srs @ACEAH, $AX0.H/srs @ACEAL, $AX0.H
	W32(DspAddress + (OrigAddress + 4) * 2, 0x02601000); // ori $AC0.M, 0x1000
}

static void DoDSPPatch( char *ptr, u32 Version )
{
	switch (Version)
	{
		case 0:		// Zelda:WW
		{
			// 5F8 - part of halt routine
			PatchZelda_Dsp( (u32)ptr, 0x05F8, 0x05B1, true, false );
			PatchZeldaInPlace_Dsp( (u32)ptr, 0x0566 );
		} break;
		case 1:		// Mario Sunshine
		{
			// E66 - unused
			PatchZelda_Dsp( (u32)ptr, 0xE66, 0x531, false, false );
			PatchZelda_Dsp( (u32)ptr, 0xE66, 0x57C, false, false );  // same orig instructions
		} break;
		case 2:		// SSBM
		{
			PatchAX_Dsp( (u32)ptr, 0x5A8, 0x65D, 0x707, 0x8F );
		} break;
		case 3:		// Mario Party 5
		{
			PatchAX_Dsp( (u32)ptr, 0x6A3, 0x758, 0x802, 0x8F );
		} break;
		case 4:		// Beyond Good and Evil
		{
			PatchAX_Dsp( (u32)ptr, 0x6E0, 0x795, 0x83F, 0x8F );
		} break;
		case 5:		// Mario Kart Double Dash
		{		
			// 5F8 - part of halt routine
			PatchZelda_Dsp( (u32)ptr, 0x05F8, 0x05B1, true, false );
			PatchZeldaInPlace_Dsp( (u32)ptr, 0x0566 );
		} break;
		case 6:		// Star Fox Assault
		{
			PatchAX_Dsp( (u32)ptr, 0x69E, 0x753, 0x814, 0xA4 );
		} break;
		case 7:		// 0 Length DSP
		{
			dbgprintf( "Error! 0 Length DSP\r\n" );
		} break;
		case 8:		// Donkey Kong Jungle Beat
		{
			// 6E5 - part of halt routine
			PatchZelda_Dsp( (u32)ptr, 0x06E5, 0x069E, true, false );
			PatchZeldaInPlace_Dsp( (u32)ptr, 0x0653 );
		} break; 
		case 9:		// Paper Mario The Thousand Year Door
		{
			PatchAX_Dsp( (u32)ptr, 0x69E, 0x753, 0x7FD, 0x8F );
		} break;
		case 10:	// Animal Crossing
		{
			// CF4 - unused
			PatchZelda_Dsp( (u32)ptr, 0x0CF4, 0x00C0, false, false );
			PatchZelda_Dsp( (u32)ptr, 0x0CF4, 0x010B, false, false );  // same orig instructions
		} break;
		case 11:	// Luigi
		{
			// BE8 - unused
			PatchZelda_Dsp( (u32)ptr, 0x0BE8, 0x00BE, false, false );
			PatchZelda_Dsp( (u32)ptr, 0x0BE8, 0x0109, false, false );  // same orig instructions
		} break;
		case 12:	// Pikmin PAL
		{
			// D2B - unused
			PatchZelda_Dsp( (u32)ptr, 0x0D2B, 0x04A8, false, true );
			PatchZelda_Dsp( (u32)ptr, 0x0D2B, 0x04F3, false, true );  // same orig instructions
		} break;
		case 13:	// IPL NTSC v1.2
		{
			// BA8 - unused
			PatchZelda_Dsp( (u32)ptr, 0x0BA8, 0x00BE, false, false );
			PatchZelda_Dsp( (u32)ptr, 0x0BA8, 0x0109, false, false );  // same orig instructions
		} break;
		case 14:	// IPL NTSC v1.0
		{
			// 938 - unused
			PatchZelda_Dsp( (u32)ptr, 0x0938, 0x00BE, false, false );
			PatchZelda_Dsp( (u32)ptr, 0x0938, 0x0109, false, false );  // same orig instructions
		} break;
		case 15:	// Metroid Prime Trilogy Wii (needed?)
		{
			PatchAX_Dsp( (u32)ptr, 0x69E, 0x753, AX_DSP_NO_DUP3, 0xA4 );
		} break;
		case 16:	// Pikmin US Demo (Unknown to Dolphin)
		{
			// DB8 - unused
			PatchZelda_Dsp( (u32)ptr, 0x0DB8, 0x00CD, false, false );
			PatchZelda_Dsp( (u32)ptr, 0x0DB8, 0x0118, false, false );  // same orig instructions
		} break;
		default:
		{
		} break;
	}
}

static bool write32A( u32 Offset, u32 Value, u32 CurrentValue, u32 ShowAssert )
{
	if( read32(Offset) != CurrentValue )
	{
		//if( ShowAssert)
			//dbgprintf("AssertFailed: Offset:%08X is:%08X should:%08X\r\n", Offset, read32(Offset), CurrentValue );
		return false;
	}

	write32( Offset, Value );
	return true;
}
void PatchB( u32 dst, u32 src )
{
	u32 newval = (dst - src);
	newval&= 0x03FFFFFC;
	newval|= 0x48000000;
	*(vu32*)src = newval;
}
void PatchBL( u32 dst, u32 src )
{
	u32 newval = (dst - src);
	newval&= 0x03FFFFFC;
	newval|= 0x48000001;
	*(vu32*)src = newval;
}

/*
	This area gets used by IOS, this workaround fixes that problem.
*/
u32 Patch31A0Backup = 0;
static void Patch31A0( void )
{
	if(Patch31A0Backup == 0)
		return;
	u32 PatchOffset = PATCH_OFFSET_START;
	//From Russia with Love stores data here not code so
	//do NOT add a jump in there, it would break the game
	if(TITLE_ID != 0x474C5A && DOLMinOff < 0x31A0)
	{
		//backup data
		u32 CurBuf = read32(0x319C);
		//create jump for it
		PatchB(PatchOffset, 0x319C);
		sync_after_write((void *)0x319C, 4);
		if ((CurBuf & 0xFC000002) == 0x40000000)
		{
			u32 Orig = CurBuf;
			u32 NewAddr = (((s16)(CurBuf & 0xFFFC)) + 0x319C - PatchOffset);
			CurBuf = (CurBuf & 0xFFFF0003) | NewAddr;
			dbgprintf("319C:Changed 0x%08X to 0x%08X\r\n", Orig, CurBuf);
		}
		else if ((CurBuf & 0xFC000002) == 0x48000000)
		{
			u32 Orig = CurBuf;
			u32 BaseAddr = (CurBuf & 0x3FFFFFC);
			if(BaseAddr & 0x2000000) BaseAddr |= 0xFC000000;
			u32 NewAddr = (((s32)BaseAddr) + 0x319C - PatchOffset) & 0x3FFFFFC;
			CurBuf = (CurBuf & 0xFC000003) | NewAddr;
			dbgprintf("319C:Changed 0x%08X to 0x%08X\r\n", Orig, CurBuf);
		}
		write32(PatchOffset, CurBuf);
		PatchOffset += 4;
	}

	u32 CurBuf = Patch31A0Backup;
	if ((CurBuf & 0xFC000002) == 0x40000000)
	{
		u32 Orig = CurBuf;
		u32 NewAddr = (((s16)(CurBuf & 0xFFFC)) + 0x31A0 - PatchOffset);
		CurBuf = (CurBuf & 0xFFFF0003) | NewAddr;
		dbgprintf("31A0:Changed 0x%08X to 0x%08X\r\n", Orig, CurBuf);
	}
	else if ((CurBuf & 0xFC000002) == 0x48000000)
	{
		u32 Orig = CurBuf;
		u32 BaseAddr = (CurBuf & 0x3FFFFFC);
		if(BaseAddr & 0x2000000) BaseAddr |= 0xFC000000;
		u32 NewAddr = (((s32)BaseAddr) + 0x31A0 - PatchOffset) & 0x3FFFFFC;
		CurBuf = (CurBuf & 0xFC000003) | NewAddr;
		dbgprintf("31A0:Changed 0x%08X to 0x%08X\r\n", Orig, CurBuf);
	}
	write32(PatchOffset, CurBuf);
	if(P2C(GameEntry) == 0x31A0) //terrible case
		GameEntry = (PatchOffset | (1<<31));
	PatchOffset += 4;
	//jump back
	PatchB(0x31A4, PatchOffset);
}

u32 PatchCopy(const u8 *PatchPtr, const u32 PatchSize)
{
	POffset -= PatchSize;
	memcpy( (void*)POffset, PatchPtr, PatchSize );
	return POffset;
}

void PatchJmpToMEM2( u32 dst, u32 src )
{
	write32(src+0x0, 0x3C008000 | (dst>>16)); //lis 0, ((1<<31)|dst)@h
	write32(src+0x4, 0x60000000 | (dst&0xFFFF)); //ori 0,0,dst@l
	write32(src+0x8, 0x7C0903A6); //mtctr 0
	write32(src+0xC, 0x4E800420); //bctr
}

void PatchFuncInterface( char *dst, u32 Length )
{
	u32 DIPatched = 0;
	u32 SIPatched = 0;
	u32 EXIPatched = 0;
	u32 AIPatched = 0;
	int i;

	u32 LISReg = 32;
	u32 LISOff = 0;
	u32 LISOp = 0;

	for( i=0; i < Length; i+=4 )
	{
		u32 op = read32( (u32)dst + i );

		if( (op & 0xFC00FFFF) == 0x3C00CC00 )	// lis rX, 0xCC00
		{
			LISReg = (op >> 21) & 0x1F;
			LISOff = (u32)dst + i;
			LISOp = op & 0xFFFF0000;
			continue;
		}

		if( (op & 0xFC1F0000) == 0x38000000 || 	// li rX, x
			(op & 0xFC1F0000) == 0x3C000000 )	// lis rX, x
		{
			u32 dstR = (op >> 21) & 0x1F;
			if (dstR == LISReg)
				LISReg = 32;
			continue;
		}

		if( (op & 0xFC00FF00) == 0x38006C00 ) // addi rX, rY, 0x6C00 (ai)
		{
			u32 src = (op >> 16) & 0x1F;
			if( src == LISReg )
			{
				write32( (u32)LISOff, LISOp | 0x0000CD80 );	// Patch to: lis rX, 0xCD80
				//dbgprintf("AI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				AIPatched++;
				LISReg = 32;
			}
			u32 dstR = (op >> 21) & 0x1F;
			if( dstR == LISReg )
				LISReg = 32;
			continue;
		}
		else if( (op & 0xFC00FF00) == 0x38006400 ) // addi rX, rY, 0x6400 (si)
		{
			u32 src = (op >> 16) & 0x1F;
			if( src == LISReg )
			{
				if (DisableSIPatch)
				{
					write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
					//dbgprintf("SI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				}
				else
				{
					write32((u32)LISOff, LISOp | 0x0000D302);	// Patch to: lis rX, 0xD302
					//dbgprintf("SI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				}
				SIPatched++;
				LISReg = 32;
			}
			u32 dstR = (op >> 21) & 0x1F;
			if( dstR == LISReg )
				LISReg = 32;
			continue;
		}
		else if( (op & 0xFC00FF00) == 0x38006800 ) // addi rX, rY, 0x6800 (exi)
		{
			u32 src = (op >> 16) & 0x1F;
			if( src == LISReg )
			{
				if( DisableEXIPatch )
				{
					write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
					//dbgprintf("EXI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					EXIPatched++;
				}
				LISReg = 32;
			}
			u32 dstR = (op >> 21) & 0x1F;
			if( dstR == LISReg )
				LISReg = 32;
			continue;
		}
		else if( (op & 0xFC00FF00) == 0x38006000 ) // addi rX, rY, 0x6000 (di)
		{
			u32 src = (op >> 16) & 0x1F;
			if( src == LISReg )
			{
				write32((u32)LISOff, LISOp | 0x0000D302);
				//dbgprintf("DI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				DIPatched++;
				LISReg = 32;
			}
			u32 dstR = (op >> 21) & 0x1F;
			if( dstR == LISReg )
				LISReg = 32;
			continue;
		}

		if ((op & 0xFC00FF00) == 0x60006C00) // ori rX, rY, 0x6C00 (ai)
		{
			u32 src = (op >> 16) & 0x1F;
			if (src == LISReg)
			{
				write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
				//dbgprintf("AI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32((u32)LISOff), LISReg);
				AIPatched++;
				LISReg = 32;
			}
			u32 dstR = (op >> 21) & 0x1F;
			if (dstR == LISReg)
				LISReg = 32;
			continue;
		}
		else if ((op & 0xFC00FF00) == 0x60006400) // ori rX, rY, 0x6400 (si)
		{
			u32 src = (op >> 16) & 0x1F;
			if (src == LISReg)
			{
				if (DisableSIPatch)
				{
					write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
					//dbgprintf("SI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32((u32)LISOff), LISReg);
				}
				else
				{
					write32((u32)LISOff, LISOp | 0x0000D302);	// Patch to: lis rX, 0xD302
					//dbgprintf("SI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32((u32)LISOff), LISReg);
				}
				SIPatched++;
				LISReg = 32;
			}
			u32 dstR = (op >> 21) & 0x1F;
			if (dstR == LISReg)
				LISReg = 32;
			continue;
		}
		else if ((op & 0xFC00FF00) == 0x60006800) // ori rX, rY, 0x6800 (exi)
		{
			u32 src = (op >> 16) & 0x1F;
			if (src == LISReg)
			{
				if ( DisableEXIPatch )
				{
					write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
					//dbgprintf("EXI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32((u32)LISOff), LISReg);
					EXIPatched++;
					LISReg = 32;
				}
			}
			u32 dstR = (op >> 21) & 0x1F;
			if (dstR == LISReg)
				LISReg = 32;
			continue;
		}
		else if ((op & 0xFC00FF00) == 0x60006000) // ori rX, rY, 0x6000 (di)
		{
			u32 src = (op >> 16) & 0x1F;
			if (src == LISReg)
			{
				write32((u32)LISOff, LISOp | 0x0000D302);
				//dbgprintf("DI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				DIPatched++;
				LISReg = 32;
			}
			u32 dstR = (op >> 21) & 0x1F;
			if (dstR == LISReg)
				LISReg = 32;
			continue;
		}
	
		if (((op & 0xF8000000) == 0x80000000)   // lwz and lwzu
		    || ( (op & 0xF8000000 ) == 0x90000000 ) ) // stw and stwu
		{
			u32 src = (op >> 16) & 0x1F;
			u32 dstR = (op >> 21) & 0x1F;
			u32 val = op & 0xFFFF;

			if( src == LISReg )
			{
				if( (val & 0xFF00) == 0x6C00 ) // case with 0x6CXY(rZ) (ai)
				{
					write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
					//dbgprintf("AI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					AIPatched++;
					LISReg = 32;
				}
				else if((val & 0xFF00) == 0x6400) // case with 0x64XY(rZ) (si)
				{
					if (DisableSIPatch)
					{
						write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
						//dbgprintf("SI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					}
					else
					{
						write32((u32)LISOff, LISOp | 0x0000D302);	// Patch to: lis rX, 0xD302
						//dbgprintf("SI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					}
					SIPatched++;
					LISReg = 32;
				}
				else if((val & 0xFF00) == 0x6800) // case with 0x68XY(rZ) (exi)
				{
					if( DisableEXIPatch )
					{
						write32((u32)LISOff, LISOp | 0x0000CD80);	// Patch to: lis rX, 0xCD80
						//dbgprintf("EXI:[%08X] %08X: lis r%u, 0xCD80\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
						EXIPatched++;
					}
					LISReg = 32;
				}
				else if((val & 0xFF00) == 0x6000) // case with 0x60XY(rZ) (di)
				{
					write32((u32)LISOff, LISOp | 0x0000D302);
					//dbgprintf("DI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					DIPatched++;
					LISReg = 32;
				}
			}
			if( dstR == LISReg )
				LISReg = 32;
			continue;
		}

		if( op == 0x4E800020 )	// blr
			LISReg = 32;
	}

	dbgprintf("Patch:[SI] applied %u times\r\n", SIPatched);
	if( DisableEXIPatch ) dbgprintf("Patch:[EXI] applied %u times\r\n", EXIPatched);
	dbgprintf("Patch:[AI] applied %u times\r\n", AIPatched);
	dbgprintf("Patch:[DI] applied %u times\r\n", DIPatched);
}

static u32 piReg = 0;
static inline bool PatchProcessorInterface( u32 BufAt0, u32 Buffer )
{
	/* Pokemon XD - PI_FIFO_WP Direct */
	if(BufAt0 == 0x80033014 && (read32(Buffer+12) & 0xFC00FFFF) == 0x540037FE) //extrwi rZ, rY, 1,5
	{
		// Extract WRAPPED bit
		u32 op = read32(Buffer+12);
		W16(Buffer+14, 0x1FFE);
		u32 src = (op >> 21) & 0x1F; //rY
		u32 dst = (op >> 16) & 0x1F; //rZ
		dbgprintf( "Patch:[PI_FIFO_WP PKM] extrwi r%i,r%i,1,2 (0x%08X)\r\n", dst, src, Buffer+12 );
		return true;
	}
	/* Dolphin SDK - GX __piReg */
	if(piReg && (BufAt0 & 0xFC0FFFFF) == (0x800D0000 | piReg)) // lwz rX, __piReg(r13)
	{
		u32 dst = (BufAt0 >> 21) & 0x1F; //rX
		u32 piRegBuf = Buffer - 4;
		u32 lPtr, sPtr;
		s32 lReg = -1, sReg = -1;
		for(lPtr = 0; lPtr < 0x20; lPtr += 4)
		{
			u32 op = read32(piRegBuf+lPtr);
			if(lReg == -1)
			{
				if((op & 0xFC00FFFF) == 0x80000014) //lwz rY, 0x14(rX)
				{
					u32 src = (op >> 16) & 0x1F;
					if(src == dst)
						lReg = (op >> 21) & 0x1F; //rY
				}
			}
			else
			{
				if((op & 0xFC00FFFF) == 0x54000188) //rlwinm rZ, rY, 0,6,4
				{
					u32 lSrc = (op >> 21) & 0x1F; //rY
					if(lSrc == lReg)
					{
						// Keep WRAPPED bit
						W16(piRegBuf+lPtr+2, 0x00C2);
						u32 lDst = (op >> 16) & 0x1F; //rZ
						dbgprintf( "Patch:[PI_FIFO_WP] rlwinm r%i,r%i,0,3,1 (0x%08X)\r\n", lDst, lSrc, piRegBuf+lPtr );
						return true;
					}
				}
				else if((op & 0xFC00FFFF) == 0x540037FE) //extrwi rZ, rY, 1,5
				{
					u32 lSrc = (op >> 21) & 0x1F; //rY
					if(lSrc == lReg)
					{
						// Extract WRAPPED bit
						W16(piRegBuf+lPtr+2, 0x1FFE);
						u32 lDst = (op >> 16) & 0x1F; //rZ
						dbgprintf( "Patch:[PI_FIFO_WP] extrwi r%i,r%i,1,2 (0x%08X)\r\n", lDst, lSrc, piRegBuf+lPtr );
						return true;
					}
				}
			}
			if(sReg == -1)
			{
				if((op & 0xFC00FFFF) == 0x54000188) //rlwinm rY, rZ, 0,6,4
				{
					sPtr = lPtr;
					sReg = (op >> 16) & 0x1F; //rY
				}
			}
			else
			{
				if((op & 0xFC00FFFF) == 0x90000014) //stw rY, 0x14(rX)
				{
					u32 sDst = (op >> 16) & 0x1F; //rX
					u32 sSrc = (op >> 21) & 0x1F; //rY
					if(sSrc == sReg && dst == sDst)
					{
						// Keep WRAPPED bit
						W16(piRegBuf+sPtr+2, 0x00C2);
						sSrc = (read32(piRegBuf+sPtr) >> 21) & 0x1F;
						dbgprintf( "Patch:[PI_FIFO_WP] rlwinm r%i,r%i,0,3,1 (0x%08X)\r\n", sReg, sSrc, piRegBuf+sPtr );
						return true;
					}
				}
			}
		}
	}
	return false;
}

// HACK: PSO 0x80001800 move to 0x931C1800
void PatchPatchBuffer(char *dst)
{
	int i;

	u32 LISReg = -1;
	u32 LISOff = -1;

	for (i = 0;; i += 4)
	{
		u32 op = read32((u32)dst + i);

		if ((op & 0xFC1FFFFF) == 0x3C008000)	// lis rX, 0x8000
		{
			LISReg = (op & 0x3E00000) >> 21;
			LISOff = (u32)dst + i;
		}

		if ((op & 0xFC000000) == 0x38000000)	// li rX, x
		{
			u32 src = (op >> 16) & 0x1F;
			u32 dst = (op >> 21) & 0x1F;
			if ((src != LISReg) && (dst == LISReg))
			{
				LISReg = -1;
				LISOff = (u32)dst + i;
			}
		}

		if ((op & 0xFC00FFFF) == 0x38001800) // addi rX, rY, 0x1800 (patch buffer)
		{
			u32 src = (op >> 16) & 0x1F;
			if (src == LISReg)
			{
				write32((u32)LISOff, (LISReg << 21) | 0x3C00931C);	// Patch to: lis rX, 0x931C
				dbgprintf("Patch:[%08X] %08X: lis r%u, 0x931C\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				LISReg = -1;
			}
			u32 dst = (op >> 21) & 0x1F;
			if (dst == LISReg)
				LISReg = -1;
		}
		if (op == 0x4E800020)	// blr
			break;
	}
}

static bool IsPokemonDemo()
{
	if( (DOLSize == 4101476 && DOLMinOff == 0x3100 && DOLMaxOff == 0x4CCFE0 &&
		read32(0x27EA58) == 0x504F4BE9 && read32(0x27EA5C) == 0x4D4F4E20) || //Colosseum EUR
		(DOLSize == 4336100 && DOLMinOff == 0x3100 && DOLMaxOff == 0x4F14C0 &&
		read32(0x2F89DC) == 0x504F4B65 && read32(0x2F89E0) == 0x4D4F4E20) || //Pokemon XD USA
		(DOLSize == 4548516 && DOLMinOff == 0x3100 && DOLMaxOff == 0x520D20 &&
		read32(0x2F3AE4) == 0x504F4B65 && read32(0x2F3AE8) == 0x4D4F4E20) )  //Pokemon XD EUR
	{
		dbgprintf("Patch:Detected Pokemon Demo, using memset patches\r\n");
		return true;
	}
	return false;
}

static bool DemoNeedsPaperMarioDMA()
{
	if( (DOLSize == 3983396 && DOLMinOff == 0x3100 && DOLMaxOff == 0x41E4E0 &&
		read32(0x2BB458) == 0x50415045 && read32(0x2BB45C) == 0x52435241) || //Paper Mario RPG JAP
		(DOLSize == 3984836 && DOLMinOff == 0x3100 && DOLMaxOff == 0x41EA80 &&
		read32(0x2BB818) == 0x50415045 && read32(0x2BB81C) == 0x52435241) || //Paper Mario 2 USA
		(DOLSize == 4019876 && DOLMinOff == 0x3100 && DOLMaxOff == 0x429140 &&
		read32(0x2BF4C0) == 0x50415045 && read32(0x2BF4C4) == 0x52435241) || //Paper Mario TTYD USA
		(DOLSize == 4068836 && DOLMinOff == 0x3100 && DOLMaxOff == 0x4359C0 &&
		read32(0x2CAF00) == 0x50415045 && read32(0x2CAF04) == 0x52435241) )  //Paper Mario TTYD EUR
	{
		dbgprintf("Patch:Known Problematic Demo, using ARStartDMA_PM\r\n");
		return true;
	}
	return false;
}

static bool DemoNeedsPostRequest()
{
	if( (DOLSize == 4107972 && DOLMinOff == 0x3100 && DOLMaxOff == 0x412760 &&
		read32(0x3768E0) == 0x53756E73 && read32(0x3768E4) == 0x68696E65) || //Sunshine JAP
		(DOLSize == 4123684 && DOLMinOff == 0x3100 && DOLMaxOff == 0x416460 &&
		read32(0x3A61AC) == 0x53756E73 && read32(0x3A61B0) == 0x68696E65) || //Sunshine EUR
		(DOLSize == 4124068 && DOLMinOff == 0x3100 && DOLMaxOff == 0x4165E0 &&
		read32(0x3A632C) == 0x53756E73 && read32(0x3A6330) == 0x68696E65) || //Sunshine USA
		(DOLSize == 3731268 && DOLMinOff == 0x3100 && DOLMaxOff == 0x4CD360 &&
		read32(0x377900) == 0x4D616E73 && read32(0x377904) == 0x696F6E00) || //Luigis Mansion EUR
		(DOLSize == 3808868 && DOLMinOff == 0x3100 && DOLMaxOff == 0x4DFE80 &&
		read32(0x38AAD0) == 0x4D616E73 && read32(0x38AAD4) == 0x696F6E00) || //Luigis Mansion USA
		(DOLSize == 3102820 && DOLMinOff == 0x3100 && DOLMaxOff == 0x3EC180 &&
		read32(0x2AA53C) == 0x50696B6D && read32(0x2AA540) == 0x696E2064) || //Pikmin EUR
		(DOLSize == 3110372 && DOLMinOff == 0x3100 && DOLMaxOff == 0x3EE780 &&
		read32(0x2AC2BC) == 0x50696B6D && read32(0x2AC2C0) == 0x696E2064) || //Pikmin USA
		(DOLSize == 3141732 && DOLMinOff == 0x3100 && DOLMaxOff == 0x3F59A0 &&
		read32(0x2B28DC) == 0x50696B6D && read32(0x2B28E0) == 0x696E2064) )  //Pikmin JAP
	{
		dbgprintf("Patch:Known Problematic Demo, using ARQPostRequest\r\n");
		return true;
	}
	return false;
}

static bool DemoNeedsHookPatch()
{
	if( (DOLSize == 2044804 && DOLMinOff == 0x3100 && DOLMaxOff == 0x2EE600 &&
		read32(0x1A9CA8) == 0x4A4F4520 && read32(0x1A9CAC) == 0x4D555354) || //Viewtiful Joe USA
		(DOLSize == 2044804 && DOLMinOff == 0x3100 && DOLMaxOff == 0x2EE600 &&
		read32(0x1A9CE8) == 0x4A4F4520 && read32(0x1A9CEC) == 0x4D555354) || //Viewtiful Joe JAP
		(DOLSize == 2556708 && DOLMinOff == 0x3100 && DOLMaxOff == 0x3732A0 &&
		read32(0x20A1D0) == 0x4A4F4520 && read32(0x20A1D4) == 0x4D555354) || //Viewtiful Joe EUR
		(DOLSize == 2176260 && DOLMinOff == 0x3100 && DOLMaxOff == 0x2D99A0 &&
		read32(0x1CF268) == 0x62696F68 && read32(0x1CF26C) == 0x617A6172) || //Biohazard 4 JAP
		(DOLSize == 2177508 && DOLMinOff == 0x3100 && DOLMaxOff == 0x2FDCE0 &&
		read32(0x1D2A00) == 0x62696F68 && read32(0x1D2A04) == 0x617A6172) || //Resident Evil 4 USA
		(DOLSize == 1118820 && DOLMinOff == 0x3100 && DOLMaxOff == 0x3C4C40 &&
		read32(0x0E1CD0) == 0x4D617269 && read32(0x0E1CD4) == 0x6F426173) || //Mario Baseball USA
		(DOLSize == 1854724 && DOLMinOff == 0x3100 && DOLMaxOff == 0x2097E0 &&
		read32(0x11A1A8) == 0x47414D45 && read32(0x11A1AC) == 0x2F496E66) )  //One Piece Treasure Battle JAP
	{
		dbgprintf("Patch:Known Problematic Demo, using ARStartDMA_Hook\r\n");
		return true;
	}
	return false;
}

static bool GameNeedsHook()
{
	if( (TITLE_ID) == 0x473258 )	// Sonic Gems Collection
		return (DOLSize != 1440644 && DOLSize != 1440100 && DOLSize != 1439812); //Everything except Sonic Fighters

	return( (TITLE_ID) == 0x474234 ||	// Burnout 2
			(TITLE_ID) == 0x47564A ||	// Viewtiful Joe
			(TITLE_ID) == 0x474145 ||	// Doubutsu no Mori e+
			(TITLE_ID) == 0x474146 ||	// Animal Crossing
			(TITLE_ID) == 0x475852 ||	// Mega Man X Command Mission
			(TITLE_ID) == 0x474832 ||	// NFS: HP2
			(TITLE_ID) == 0x474156 ||	// Avatar Last Airbender
			(TITLE_ID) == 0x47484E ||	// Hunter the Reckoning
			(TITLE_ID) == 0x473442 ||	// Resident Evil 4
			(TITLE_ID) == 0x474856 ||	// Disneys Hide and Sneak
			(TITLE_ID) == 0x474353 ||	// Street Racing Syndicate
			(TITLE_ID) == 0x474241 ||	// NBA 2k2
			(TITLE_ID) == 0x47414D ||	// Army Men Sarges War
			(TITLE_ID) == 0x474D4C ||	// ESPN MLS Extra Time 2002
			(TITLE_ID) == 0x474D5A ||	// Monster 4x4: Masters Of Metal
			(TITLE_ID) == 0x47504C ||	// Piglet's Big Game
			(TITLE_ID) == 0x475951 ||	// Mario Superstar Baseball
			(TITLE_ID) == 0x47534F ||	// Sonic Mega Collection
			(TITLE_ID) == 0x474244 ||	// BloodRayne
			(TITLE_ID) == 0x475438 ||	// Big Mutha Truckers
			(TITLE_ID) == 0x47444D ||	// Disney Magical Mirror Starring Mickey Mouse
			(TITLE_ID) == 0x475153 ||	// Tales of Symphonia
			(TITLE_ID) == 0x474645 ||	// Fire Emblem
			(TITLE_ID) == 0x47414C ||	// Super Smash Bros. Melee
			(GAME_ID) == 0x474F544A ||	// One Piece - Treasure Battle
			(GAME_ID) == 0x4747504A ||	// SD Gundam Gashapon Wars
			DemoNeedsHookPatch() );
}

static inline bool PADSwitchRequired()
{
	return( (TITLE_ID) == 0x47434F ||	// Call of Duty
                        (TITLE_ID) == 0x475449 ||	// Tiger Woods PGA Tour 2003
			(TITLE_ID) == 0x475734 );	// Tiger Woods PGA Tour 2004
			
}

static inline bool PADForceConnected()
{
	return( (TITLE_ID) == 0x474C5A || // 007 From Russia With Love
                        (TITLE_ID) == 0x474153 ||	// sonic dx jap
                        (TITLE_ID) == 0x475853 ||	// sonic dx usa and pal
                        (TITLE_ID) == 0x475735 ||	// Need For Speed Carbon
			(TITLE_ID) == 0x474f57 );	// Nedd For Speed Most Wanted
}

static inline bool GameRelTimerPatches()
{
	return( (TITLE_ID) == 0x474842 || // The Hobbit
			(TITLE_ID) == 0x475536 || // Nicktoons Battle for Volcano Island
			(TITLE_ID) == 0x474E4F || // Nicktoons Unite
			(TITLE_ID) == 0x475941 || // Nickelodeon Barnyard
			(TITLE_ID) == 0x474F32 ); // Blood Omen 2
}

static inline bool GameRelConstantTimerPatches()
{
	return( (TITLE_ID) == 0x474F32 ); // Blood Omen 2
}

void MPattern(u8 *Data, u32 Length, FuncPattern *FunctionPattern)
{
	u32 i;

	memset32( FunctionPattern, 0, sizeof(FuncPattern) );

	for( i = 0; i <= Length; i+=4 )
	{
		u32 word = *(u32*)(Data + i);

		if( (word & 0xFC000003) == 0x48000001 )
			FunctionPattern->FCalls++;

		if( (word & 0xFC000003) == 0x48000000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) == 0x40800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) == 0x41800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) == 0x40810000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) == 0x41820000 )
			FunctionPattern->Branch++;

		if( (word & 0xFC000000) == 0x80000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) == 0x38000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) == 0x3C000000 )
			FunctionPattern->Loads++;

		if( (word & 0xFC000000) == 0x90000000 )
			FunctionPattern->Stores++;
		if( (word & 0xFC000000) == 0x94000000 )
			FunctionPattern->Stores++;

		if( (word & 0xFF000000) == 0x7C000000 )
			FunctionPattern->Moves++;

		if( word == 0x4E800020 )
			break;
	}

	FunctionPattern->Length = i;
}

int CPattern( FuncPattern *FPatA, FuncPattern *FPatB  )
{
	return ( memcmp( FPatA, FPatB, sizeof(u32) * 6 ) == 0 );
}

#ifdef DEBUG_PATCH
static const char *getVidStr(u32 in)
{
	switch(in)
	{
		case VI_NTSC:
			return "NTSC";
		case VI_PAL:
			return "PAL50";
		case VI_EUR60:
			return "PAL60";
		case VI_MPAL:
			return "PAL-M";
		case VI_480P:
			return "480p";
		default:
			return "Unknown Video Mode";
	}
}

static inline void printvidpatch(u32 ori_v, u32 new_v, u32 addr)
{
	dbgprintf("Patch:Replaced %s with %s (0x%08X)\r\n", getVidStr(ori_v), getVidStr(new_v), addr );
}
static inline void printpatchfound(const char *name, const char *type, u32 offset)
{
	if(type) dbgprintf("Patch:[%s %s] applied (0x%08X)\r\n", name, type, offset);
	else dbgprintf("Patch:[%s] applied (0x%08X)\r\n", name, offset);
}
#else
#define printvidpatch(...)
#define printpatchfound(...)
#endif

static int GotoFuncStart(int i, u32 Buffer)
{
	while( read32(Buffer + i - 4) != 0x4E800020 )
		i -= 4;
	return i;
}

static int GotoFuncEnd(int i, u32 Buffer)
{
	do {
		i += 4; //known function, skip over it
	} while( read32(Buffer + i) != 0x4E800020 );
	return i;
}

/**
 * Does the specified file exist?
 * @param path Filename.
 * @return True if the file exists; false if it doesn't.
 */
static bool fileExist(const char *path)
{
	FIL fd;
	if (f_open_char(&fd, path, FA_READ|FA_OPEN_EXISTING) == FR_OK)
	{
		f_close(&fd);
		return true;
	}
	return false;
}

void DoPatches( char *Buffer, u32 Length, u32 DiscOffset )
{
	if( (u32)Buffer == 0x01200000 && *(u8*)Buffer == 0x7C )
	{	/* Game can reset at any time */
		dbgprintf("DIP:Apploader, preparing to patch\r\n");
		PatchState = PATCH_STATE_DONE;
		return;
	}
	//dbgprintf("%08x %08x\r\n",Length,DiscOffset);
	int i, j, k;
	u32 read;
	u32 value = 0;

	// PSO 1&2 / III
	u32 isPSO = 0;
	if (((TITLE_ID) == 0x44504F) || ((TITLE_ID) == 0x47504F) || ((TITLE_ID) == 0x475053))
	{
		isPSO = 1;
		if((PSOHack & PSO_STATE_SWITCH) && DiscOffset > 0)
		{
			dbgprintf("PSO:psov3.dol/iwanaga.dol\r\n");
			PSOHack = PSO_STATE_LOAD | PSO_STATE_NOENTRY;
		}
		if( (Length == 0x318E0 && read32((u32)Buffer+0x318B0) == 0x4CBEBC20) || //PSO PAL/NTSC v1.00/v1.01
			(Length == 0x31EC0 && read32((u32)Buffer+0x319E0) == 0x4CBEBC20) || //PSO Plus NTSC v1.02
			(Length == 0x31B60 && read32((u32)Buffer+0x31B28) == 0x4CBEBC20) || //PSO JAP v1.02/v1.03
			(Length == 0x339A0 && read32((u32)Buffer+0x33960) == 0x4CBEBC20) || //PSO Plus JAP v1.05
			(Length == 0x33AC0 && read32((u32)Buffer+0x33A80) == 0x4CBEBC20) || //PSO 3 NTSC
			(Length == 0x33C20 && read32((u32)Buffer+0x33BE8) == 0x4CBEBC20) || //PSO 3 PAL
			(Length == 0x33FC0 && read32((u32)Buffer+0x33F78) == 0x4CBEBC20) || //PSO 3 JAP
			(Length == 0x34B60 && read32((u32)Buffer+0x34B28) == 0x4CBEBC20) || //PSO Demo JAP
			//this case is a little weird, file is actually full switcher, but game reads it in multiple parts,
			//so just watch out for the 1st part being read in, should be good enough I suppose
			(Length == 0x31800 && read32((u32)Buffer+0x2A410) == 0x0A417070) ||	//PSO PAL/NTSC v1.00/v1.01
			(Length == 0x31800 && read32((u32)Buffer+0x2A790) == 0x0A417070) ||	//PSO Plus NTSC v1.02
			(Length == 0x31800 && read32((u32)Buffer+0x2A0D0) == 0x0A417070) ||	//PSO JAP v1.02/v1.03
			(Length == 0x33800 && read32((u32)Buffer+0x2B830) == 0x0A417070) ||	//PSO Plus JAP v1.05
			(Length == 0x33800 && read32((u32)Buffer+0x2C170) == 0x0A417070) ||	//PSO 3 NTSC
			(Length == 0x33800 && read32((u32)Buffer+0x2C370) == 0x0A417070) ||	//PSO 3 PAL
			(Length == 0x33800 && read32((u32)Buffer+0x2C010) == 0x0A417070))	//PSO 3 JAP
		{
			dbgprintf("PSO:switcher.dol start load\r\n");
			PSOHack = PSO_STATE_LOAD | PSO_STATE_SWITCH_START;
		}
		else if(Length == 0x1B8A0 && read32((u32)Buffer+0x12E0C) == 0x7FA4E378) //All PSO Versions?
		{
			dbgprintf("PSO:switcherD.dol start load\r\n");
			PSOHack = PSO_STATE_LOAD | PSO_STATE_SWITCH_START;
		}
		else if((Length == 0x19580 && read32((u32)Buffer+0x19560) == 0xC8BFAF78) || //PSO Plus NTSC v1.02
				(Length == 0x1A5C0 && read32((u32)Buffer+0x1A5A0) == 0x4CA7BEBC) || //PSO Plus JAP v1.05
				(Length == 0x19EA0 && read32((u32)Buffer+0x19E80) == 0x24C7E996) || //PSO 3 PAL
				(Length == 0x1A2C0 && read32((u32)Buffer+0x1A2A0) == 0xE2BEE1FF) || //PSO 3 NTSC
				(Length == 0x1A960 && read32((u32)Buffer+0x1A940) == 0x09F8FF13))   //PSO 3 JAP
		{
			dbgprintf("PSO:switcher.prs start load\r\n");
			PSOHack = PSO_STATE_LOAD | PSO_STATE_PSR | PSO_STATE_SWITCH_START;
		}
	}

	if ((TITLE_ID) == 0x474E48)  //NHL Hitz = Possible Datel
	{
		if ((Length >= 0x19850)
		     && read32((u32)Buffer+0x19848-0x2460) == 0x20446174
                     && ((read32((u32)Buffer+0x1984C-0x2460) >> 16) == 0x656C))
		{
			Datel = true;
		}
		if (Datel)
		{
			switch ((u32)Buffer)
			{
				case 0x01300000:	// Datel AppLoader
				{
				} break;
				case 0x00003100:	// Datel DOL
				{
					dbgprintf("Patch:Datel loading dol:0x%p %u\r\n", Buffer, Length);
					DOLSize = Length;
					DOLMinOff = (u32)Buffer;
					DOLMaxOff = DOLMinOff + Length;
					Datel = true;
					if (Length == 0x000BA000)
					{
						GameEntry = 0x81300000;
						PatchState = PATCH_STATE_PATCH;
					}
					else
						GameEntry = DOLMinOff | 0x80000000;
				} break;
			}
		}
	}

	if ((PatchState & 0x3) == PATCH_STATE_NONE)
	{
		if (Length == 0x100 || (PSOHack & PSO_STATE_LOAD))
		{
			PSOHack &= (~PSO_STATE_LOAD);
			void *tmpbuf = NULL;
			if(PSOHack & PSO_STATE_PSR)
			{
				u32 tmpsize = prs_decompress_size(Buffer);
				dbgprintf("PSO:Decompressing PRS with size %08x\r\n",tmpsize);
				tmpbuf = malloc(tmpsize); //not too big
				prs_decompress(Buffer,tmpbuf);
				Buffer = tmpbuf; //better than writing the stuff below again
			}
			if( read32( (u32)Buffer ) == 0x100 && (((dolhdr*)Buffer)->entrypoint & 0xFE000000) == 0x80000000 )
			{
				ELFLoading = 0;
				//quickly calc the size
				DOLSize = sizeof(dolhdr);
				dolhdr *dol = (dolhdr*)Buffer;

				for( i=0; i < 7; ++i )
					DOLSize += dol->sizeText[i];
				for( i=0; i < 11; ++i )
					DOLSize += dol->sizeData[i];

				DOLMinOff=0x81800000;
				DOLMaxOff=0;

				for( i=0; i < 7; ++i )
				{
					if( dol->addressText[i] == 0 )
						continue;

					if( DOLMinOff > dol->addressText[i])
						DOLMinOff = dol->addressText[i];

					if( DOLMaxOff < dol->addressText[i] + dol->sizeText[i] )
						DOLMaxOff = dol->addressText[i] + dol->sizeText[i];
				}

				for( i=0; i < 11; ++i )
				{
					if( dol->addressData[i] == 0 )
						continue;

					if( DOLMinOff > dol->addressData[i])
						DOLMinOff = dol->addressData[i];

					if( DOLMaxOff < dol->addressData[i] + dol->sizeData[i] )
						DOLMaxOff = dol->addressData[i] + dol->sizeData[i];
				}

				DOLMinOff -= 0x80000000;
				DOLMaxOff -= 0x80000000;

				//if (PSOHack == PSO_STATE_LOAD)
				//{
				//	DOLMinOff = (u32)Buffer;
				//	DOLMaxOff = (u32)Buffer + DOLSize;
				//}
//#ifdef DEBUG_DI
				dbgprintf("DIP:DOL Size:%d MinOff:0x%08X MaxOff:0x%08X\r\n", DOLSize, DOLMinOff, DOLMaxOff );
//#endif
				/* Hack Position */
				GameEntry = dol->entrypoint;
				if (!(PSOHack & PSO_STATE_NOENTRY))
					dol->entrypoint = PATCH_OFFSET_ENTRY + 0x80000000;
				else
					PSOHack &= (~PSO_STATE_NOENTRY);
				dbgprintf("DIP:DOL EntryPoint::0x%08X, GameEntry::0x%08X\r\n", dol->entrypoint, GameEntry);
				PatchState |= PATCH_STATE_LOAD;
			}
			if(PSOHack & PSO_STATE_PSR)
			{
				free(tmpbuf);
				/* Let it patch on PPC side */
				write32(PRS_DOL, PATCH_OFFSET_ENTRY + 0x80000000);
				sync_after_write((void*)PRS_DOL, 0x20);
				PSOHack &= (~PSO_STATE_PSR);
				return;
			}
		}
		else if (read32((u32)Buffer) == 0x7F454C46 && ((Elf32_Ehdr*)Buffer)->e_phnum)
		{
			ELFLoading = 1;
#ifdef DEBUG_DI
			dbgprintf("DIP:Game is loading an ELF 0x%08x (%u)\r\n", DiscOffset, Length );
#endif
			DOLSize = 0;

			Elf32_Ehdr *ehdr = (Elf32_Ehdr*)Buffer;
#ifdef DEBUG_DI
			dbgprintf("DIP:ELF Start:0x%08X Programheader Entries:%u\r\n", ehdr->e_entry, ehdr->e_phnum );
#endif
			DOLMinOff = 0x81800000;
			DOLMaxOff = 0;
			for( i=0; i < ehdr->e_phnum; ++i )
			{
				Elf32_Phdr *phdr = (Elf32_Phdr*)(Buffer + ehdr->e_phoff + (i * sizeof(Elf32_Phdr)));
#ifdef DEBUG_DI
				dbgprintf("DIP:ELF Programheader:0x%08X\r\n", phdr );
#endif
				if( DOLMinOff > phdr->p_vaddr)
					DOLMinOff = phdr->p_vaddr;

				if( DOLMaxOff < phdr->p_vaddr + phdr->p_filesz )
					DOLMaxOff = phdr->p_vaddr + phdr->p_filesz;

				DOLSize += (phdr->p_filesz+31) & (~31);	// align by 32byte
			}
			DOLMinOff -= 0x80000000;
			DOLMaxOff -= 0x80000000;
//#ifdef DEBUG_DI
			dbgprintf("DIP:ELF Size:%d MinOff:0x%08X MaxOff:0x%08X\r\n", DOLSize, DOLMinOff, DOLMaxOff );
//#endif
			/* Hack Position */
			GameEntry = ehdr->e_entry;
			ehdr->e_entry = PATCH_OFFSET_ENTRY + 0x80000000;
			PatchState |= PATCH_STATE_LOAD;
		}
		else if(read32((u32)Buffer+0xC) == 0x7F454C46)
		{
			GameEntry = read32((u32)Buffer+0x20);
			DOLMinOff = P2C(GameEntry);
			DOLMaxOff = P2C(read32((u32)Buffer+0xA0));
			DOLSize = DOLMaxOff - DOLMinOff;
			u32 NewVal = (PATCH_OFFSET_ENTRY - P2C(GameEntry));
			NewVal &= 0x03FFFFFC;
			NewVal |= 0x48000000;
			FirstLine = read32((u32)Buffer+0xB4);
			write32((u32)Buffer+0xB4, NewVal);
			dbgprintf("DIP:Hacked ELF FirstLine:0x%08X Size:%d MinOff:0x%08X MaxOff:0x%08X\r\n", FirstLine, DOLSize, DOLMinOff, DOLMaxOff );
			PatchState |= PATCH_STATE_LOAD;
		}
		else if( (u32)Buffer == 0x01300000 && *(vu8*)Buffer == 0x48 )
		{
			GameEntry = 0x81300000;
			DOLMinOff = P2C(GameEntry);
			DOLMaxOff = DOLMinOff + Length;
			DOLSize = DOLMaxOff - DOLMinOff;
			u32 NewVal = (PATCH_OFFSET_ENTRY - P2C(GameEntry));
			NewVal &= 0x03FFFFFC;
			NewVal |= 0x48000000;
			FirstLine = read32((u32)Buffer);
			write32((u32)Buffer, NewVal);
			dbgprintf("DIP:App Switcher Size:%d MinOff:0x%08X MaxOff:0x%08X\r\n", DOLSize, DOLMinOff, DOLMaxOff);
			PatchState |= PATCH_STATE_LOAD;
			/* Make sure to backup ingame settings */
			TRIBackupSettings();
		}
	}

	/* No Application patches, check for general patches */
	if (!(PatchState & PATCH_STATE_PATCH))
	{
		/* Fixes Segaboot Boot Caution 51 */
		if( TRIGame == TRI_SB && Length == 0x1C0 && *((vu8*)Buffer+0x0F) == 0x06 )
		{
			*((vu8*)Buffer+0x0F) = 0x07;
			dbgprintf("TRI:Patched boot.id Video Mode\r\n");
		} /* Modify Sonic Riders _Main.rel not crash on certain mem copies */
		else if( (GAME_ID) == 0x47584545 && (u32)Buffer == SONICRIDERS_BASE_NTSCU
			&& Length == 0x80000 && read32(SONICRIDERS_HOOK_NTSCU) == 0x7CFF3B78 )
		{
			PatchBL(PatchCopy(SonicRidersCopy, SonicRidersCopy_size), SONICRIDERS_HOOK_NTSCU);
			dbgprintf("Patch:Patched Sonic Riders _Main.rel NTSC-U\r\n");
		}
		else if( (GAME_ID) == 0x4758454A && (u32)Buffer == SONICRIDERS_BASE_NTSCJ
			&& Length == 0x80000 && read32(SONICRIDERS_HOOK_NTSCJ) == 0x7CFF3B78 )
		{
			PatchBL(PatchCopy(SonicRidersCopy, SonicRidersCopy_size), SONICRIDERS_HOOK_NTSCJ);
			dbgprintf("Patch:Patched Sonic Riders _Main.rel NTSC-J\r\n");
		}
		else if( (GAME_ID) == 0x47584550 && (u32)Buffer == SONICRIDERS_BASE_PAL
			&& Length == 0x80000 && read32(SONICRIDERS_HOOK_PAL) == 0x7CFF3B78 )
		{
			PatchBL(PatchCopy(SonicRidersCopy, SonicRidersCopy_size), SONICRIDERS_HOOK_PAL);
			dbgprintf("Patch:Patched Sonic Riders _Main.rel PAL\r\n");
		} /* Agressive Timer Patches for Nintendo Puzzle Collection */
		else if( (TITLE_ID) == 0x47505A && useipl == 0 )
		{
			u32 t;
			for(t = 0; t < Length; t+=4) //make sure its patched at all times
				PatchTimers(read32((u32)Buffer+t), (u32)Buffer+t, true);
		} /* Patch .rel file on boot */
		else if(NeedRelPatches)
		{
			u32 t;
			for(t = 0; t < Length; t+=4)
			{
				//only look for .rel code to patch, no floats
				if(PatchTimers(read32((u32)Buffer+t), (u32)Buffer+t, false))
				{
					//some games constantly reload .rel files
					if(!NeedConstantRelPatches)
						NeedRelPatches = 0;
				}
			}
			if(NeedRelPatches == 0)
				dbgprintf("Patch:Patched .rel Timers\r\n");
		}
		/* Checked files, back to normal handling */
		return;
	}

	/* Application ready to get patched */
	u32 DSPHandlerNeeded = 1;
	IsN64Emu = 0;
	piReg = 0;

	sync_after_write(Buffer, Length);

	Buffer = (char*)DOLMinOff;
	Length = DOLMaxOff - DOLMinOff;

	dbgprintf("Patch:Offset:0x%08X EOffset:0x%08X Length:%08X\r\n", Buffer, DOLMaxOff, Length );
	dbgprintf("Patch:Game ID = %x\r\n", GAME_ID);

	/* for proper dol detection in PSO */
	if(isPSO && (PSOHack & PSO_STATE_SWITCH_START))
	{
		dbgprintf("PSO:switcher end load\r\n");
		PSOHack &= ~PSO_STATE_SWITCH_START;
		PSOHack |= PSO_STATE_SWITCH;
	}
	/* requires lots of additional timer patches for BBA */
	isKirby = (TITLE_ID == 0x474B59);
        if(TITLE_ID == 0x474458)
       isdisneyskt = 0x474458;

	sync_before_read(Buffer, Length);

	/* Have gc patches loaded at the same place for reloading games */
	POffset = PATCH_OFFSET_ENTRY;

	/* Most important patch, needed for every game read activity */
	u32 __DVDInterruptHandlerAddr = PatchCopy(__DVDInterruptHandler, __DVDInterruptHandler_size);
	/* Patch to make sure PAD is not inited too early */
	u32 SIInitStoreAddr = DisableSIPatch ? 0 : PatchCopy(SIInitStore, SIInitStore_size);
	/* Patch for soft-resetting with a button combination */
	u32 FakeRSWLoadAddr = DisableSIPatch ? 0 : PatchCopy(FakeRSWLoad, FakeRSWLoad_size);
	u32 FakeRSWStoreAddr = DisableSIPatch ? 0 : PatchCopy(FakeRSWStore, FakeRSWStore_size);
	/* Datels own timer */
	u32 DatelTimerAddr = Datel ? PatchCopy(DatelTimer, DatelTimer_size) : 0;

	TRISetupGames();
	if(TRIGame == TRI_NONE)
	{
		if(( TITLE_ID == 0x475858 ) || ( TITLE_ID == 0x474336 ) // Colosseum and XD
			|| ( GAME_ID == 0x5043534A && Length == 0x479900 ) // Colosseum Bonus
			|| IsPokemonDemo() )
		{
			dbgprintf("Patch:[Pokemon memset] applied\r\n");

			// patch memset to jump to test function
			write32(0x00005420, 0x4BFFAC68);

			// patch in test < 0x3000 function
			write32(0x00000088, 0x3CC08000);
			write32(0x0000008C, 0x60C63000);
			write32(0x00000090, 0x7C033000);
			write32(0x00000094, 0x41800008);
			write32(0x00000098, 0x480053A5);
			write32(0x0000009C, 0x48005388);
		}
		else if( TITLE_ID == 0x47465A && read32(0x5608) == 0x3C804C00 )
		{
			dbgprintf("Patch:[F-Zero GX Low Mem] applied\r\n");
			// dont write "rfi" into 0x1000, 0x1100, 0x1200 and 0x1300
			if( (read32(0) & 0xFF) == 0x4A )	// JAP
				memset32( (void*)0x5630, 0x60000000, 0x10 );
			else								// EUR/USA
				memset32( (void*)0x5634, 0x60000000, 0x10 );
		}
		else if( TITLE_ID == 0x475053 )
		{
			// HACK: PSO 0x80001800 move to 0x931C1800
			if(read32(0x590F4) == 0x3C601000 && read32(0x590FC) == 0x3B830300)
			{
				dbgprintf("Patch:[PSO Ep.III NTSC-U 0x931C] applied\r\n");
				u32 shifted_lowmem = 0x931C1800 >> 3;
				write32(0x590F4, 0x3C600000 | (shifted_lowmem>>16));
				write32(0x590FC, 0x607C0000 | (shifted_lowmem&0xFFFF));
			}
			else if(read32(0x5925C) == 0x3C601000 && read32(0x59264) == 0x3B830300)
			{
				dbgprintf("Patch:[PSO Ep.III PAL 0x931C] applied\r\n");
				u32 shifted_lowmem = 0x931C1800 >> 3;
				write32(0x5925C, 0x3C600000 | (shifted_lowmem>>16));
				write32(0x59264, 0x607C0000 | (shifted_lowmem&0xFFFF));
			}
			else if(read32(0x591A0) == 0x3C601000 && read32(0x591A8) == 0x3B830300)
			{
				dbgprintf("Patch:[PSO Ep.III NTSC-J 0x931C] applied\r\n");
				u32 shifted_lowmem = 0x931C1800 >> 3;
				write32(0x591A0, 0x3C600000 | (shifted_lowmem>>16));
				write32(0x591A8, 0x607C0000 | (shifted_lowmem&0xFFFF));
			}
		}
		else if(DOLMaxOff == 0x141FE0 && read32(0xF278C) == 0x2F6D616A) //Majoras Mask NTSC-U
		{
			dbgprintf("Patch:[Majoras Mask NTSC-U] applied\r\n");
			//save up regs
			u32 majora_save = PatchCopy(MajoraSaveRegs, MajoraSaveRegs_size);
			PatchB(majora_save, 0x1CDD4);
			PatchB(0x1CDD8, majora_save+MajoraSaveRegs_size-4);
			//frees r28 and secures r10 for us
			write32(0x1CE8C, 0x60000000);
			write32(0x1CE90, 0x839D0000);
			write32(0x1CE94, 0x7D3C4AAE);
			//do audio streaming injection
			u32 majora_as = PatchCopy(MajoraAudioStream, MajoraAudioStream_size);
			PatchB(majora_as, 0x1CEA4);
			PatchB(0x1CEA8, majora_as+MajoraAudioStream_size-4);
			//load up regs (and jump back)
			PatchB(PatchCopy(MajoraLoadRegs, MajoraLoadRegs_size), 0x1CF98);
			DSPHandlerNeeded = 0;
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x1467C0 && read32(0xF624C) == 0x2F6D616A) //Majoras Mask NTSC-J
		{
			dbgprintf("Patch:[Majoras Mask NTSC-J] applied\r\n");
			//save up regs
			u32 majora_save = PatchCopy(MajoraSaveRegs, MajoraSaveRegs_size);
			PatchB(majora_save, 0x1D448);
			PatchB(0x1D44C, majora_save+MajoraSaveRegs_size-4);
			//frees r28 and secures r10 for us
			write32(0x1D500, 0x60000000);
			write32(0x1D504, 0x839D0000);
			write32(0x1D508, 0x7D3C4AAE);
			//do audio streaming injection
			u32 majora_as = PatchCopy(MajoraAudioStream, MajoraAudioStream_size);
			PatchB(majora_as, 0x1D518);
			PatchB(0x1D51C, majora_as+MajoraAudioStream_size-4);
			//load up regs (and jump back)
			PatchB(PatchCopy(MajoraLoadRegs, MajoraLoadRegs_size), 0x1D60C);
			DSPHandlerNeeded = 0;
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x130C40 && read32(0xE16D8) == 0x2F6D616A) //Majoras Mask PAL
		{
			dbgprintf("Patch:[Majoras Mask PAL] applied\r\n");
			//save up regs
			u32 majora_save = PatchCopy(MajoraSaveRegs, MajoraSaveRegs_size);
			PatchB(majora_save, 0x1D6B4);
			PatchB(0x1D6B8, majora_save+MajoraSaveRegs_size-4);
			//frees r28 and secures r10 for us
			write32(0x1D76C, 0x60000000);
			write32(0x1D770, 0x839D0000);
			write32(0x1D774, 0x7D3C4AAE);
			//do audio streaming injection
			u32 majora_as = PatchCopy(MajoraAudioStream, MajoraAudioStream_size);
			PatchB(majora_as, 0x1D784);
			PatchB(0x1D788, majora_as+MajoraAudioStream_size-4);
			//load up regs (and jump back)
			PatchB(PatchCopy(MajoraLoadRegs, MajoraLoadRegs_size), 0x1D878);
			DSPHandlerNeeded = 0;
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x138920 && read32(0x1373E8) == 0x5A454C44) //Ocarina of Time NTSC-U
		{
			dbgprintf("Patch:[Ocarina of Time NTSC-U]\r\n");
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x136420 && read32(0x134EE8) == 0x5A454C44) //Ocarina of Time NTSC-J
		{
			dbgprintf("Patch:[Ocarina of Time NTSC-J]\r\n");
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x182760 && read32(0x181200) == 0x5A454C44) //Ocarina of Time PAL
		{
			dbgprintf("Patch:[Ocarina of Time PAL]\r\n");
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x136BC0 && read32(0x1356B8) == 0x5A454C44) //OOT Bonus Disc NTSC-U
		{
			dbgprintf("Patch:[OOT Bonus Disc NTSC-U]\r\n");
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x18C660 && read32(0x18B178) == 0x5A454C44) //OOT Bonus Disc NTSC-J
		{
			dbgprintf("Patch:[OOT Bonus Disc NTSC-J]\r\n");
			IsN64Emu = 1;
		}
		else if(DOLMaxOff == 0x159800 && read32(0x1582F0) == 0x5A454C44) //OOT Bonus Disc PAL
		{
			dbgprintf("Patch:[OOT Bonus Disc PAL]\r\n");
			IsN64Emu = 1;
		}
	}
	DisableEXIPatch = (TRIGame == TRI_NONE && ConfigGetConfig(NIN_CFG_MEMCARDEMU) == false);
	DisableSIPatch = (!IsWiiU() && TRIGame == TRI_NONE && ConfigGetConfig(NIN_CFG_NATIVE_SI));

	bool PatchWide = ConfigGetConfig(NIN_CFG_FORCE_WIDE);
	if(PatchWide && PatchStaticWidescreen(TITLE_ID, GAME_ID & 0xFF)) //if further patching is needed
		PatchWide = false;

	PatchFuncInterface( Buffer, Length );

	u32 PatchCount = FPATCH_VideoModes | FPATCH_VIConfigure | FPATCH_getTiming |
		FPATCH_OSSleepThread | FPATCH_GXBegin | FPATCH_GXDrawDone;
#ifdef CHEATS
	u32 cheatsWanted = 0, debuggerWanted = 0;
	if(ConfigGetConfig(NIN_CFG_CHEATS))
		cheatsWanted = 1;
	if(!IsWiiU() && ConfigGetConfig(NIN_CFG_DEBUGGER))
		debuggerWanted = 1;
	if(cheatsWanted || debuggerWanted)
		PatchCount &= ~FPATCH_OSSleepThread;
	/* So this can be used but for now we just use PADRead */
	/*if( (IsWiiU() && ConfigGetConfig(NIN_CFG_CHEATS)) ||
		(!IsWiiU() && ConfigGetConfig(NIN_CFG_DEBUGGER|NIN_CFG_CHEATS)) )
	{
		PatchCount &= ~(
			FPATCH_OSSleepThread | //Hook 1
			//FPATCH_GXBegin //Hook 2, unstable!
			FPATCH_GXDrawDone //Hook 3
		);
	}*/
#endif
	u32 videoPatches = 0;
	if( ConfigGetConfig(NIN_CFG_FORCE_PROG) || (ConfigGetVideoMode() & NIN_VID_FORCE) ||
		(ConfigGetVideoOffset() != 0 && ConfigGetVideoOffset() >= -20 && ConfigGetVideoOffset() <= 20) ||
		(ConfigGetVideoScale() != 0 && ConfigGetVideoScale() >= 40 && ConfigGetVideoScale() <= 120) )
	{
		PatchCount &= ~(FPATCH_VideoModes | FPATCH_VIConfigure | FPATCH_getTiming);
		videoPatches = 1;
	}

	if(ConfigGetConfig(NIN_CFG_BBA_EMU))
	{
		if(TITLE_ID == 0x474D34 || TITLE_ID == 0x474B59 || TITLE_ID == 0x475445)
			bbaEmuWanted = 1;
		else if(TITLE_ID == 0x474845 || isPSO)
			bbaEmuWanted = 2;
	}
	else
		bbaEmuWanted = 0;

	if(bbaEmuWanted)
		PatchCount &= ~FPATCH_OSSleepThread;
	/* Set up patch pattern lists */
	FuncPatterns CurFPatternsList[PCODE_MAX];
	u32 CurFPatternsListLen = 0;
	u32 minPatternSize = 0xFFFFFFFF;
	u32 maxPatternSize = 0;
	u32 patitr;
	for(patitr = 0; patitr < PCODE_MAX; ++patitr)
	{
		/* only handle used patterns */
		if(AllFPatterns[patitr].patmode == PCODE_TRI && TRIGame == TRI_NONE)
			continue;
		if(AllFPatterns[patitr].patmode == PCODE_EXI && DisableEXIPatch)
			continue;
		if (AllFPatterns[patitr].patmode == PCODE_DATEL && Datel == 0)
			continue;
		if (AllFPatterns[patitr].patmode == PCODE_PSO && isPSO == 0)
			continue;
		if (AllFPatterns[patitr].patmode == PCODE_PSOSO && bbaEmuWanted != 2)
			continue;
		if (AllFPatterns[patitr].patmode == PCODE_NINSO && bbaEmuWanted != 1)
			continue;
		if (AllFPatterns[patitr].patmode == PCODE_SI && DisableSIPatch)
			continue;
		/* clear from pervious patches */
		FuncPattern *CurPatterns = AllFPatterns[patitr].pat;
		u32 CurPatternsLen = AllFPatterns[patitr].patlen;
		for( j=0; j < CurPatternsLen; ++j )
		{
			CurPatterns[j].Found = 0;
			if(CurPatterns[j].Length < minPatternSize)
				minPatternSize = CurPatterns[j].Length;
			if(CurPatterns[j].Length > maxPatternSize)
				maxPatternSize = CurPatterns[j].Length;
		}
		/* we use this pattern type */
		CurFPatternsList[CurFPatternsListLen].pat = CurPatterns;
		CurFPatternsList[CurFPatternsListLen].patlen = CurPatternsLen;
		CurFPatternsListLen++;
	}
	/* Cheats */
	u32 OSSleepThreadHook = 0;
	u32 PADHook = 0;
	/* BBA Emu Stuff */
	u32 OSSleepThread = 0, OSWakeupThread = 0;
	s16 SOStartedOffset = 0;
	/* PSO BBA Emu Stuff */
	u32 OSCreateThread = 0, OSResumeThread = 0;
	s16 DHCPStateOffset = 0;
	//u32 DebuggerHook = 0, DebuggerHook2 = 0, DebuggerHook3 = 0;
	/* SI Inited Patch */
	u32 PADInitOffset = 0, SIInitOffset = 0;
	/* DSP Patches */
	u8 *SHA1i = (u8*)malloca( 0x60, 0x40 );
	u8 *hash  = (u8*)malloca( 0x14, 0x40 );
	/* Widescreen Hacks */
	u32 MTXPerspectiveOffset = 0, MTXLightPerspectiveOffset = 0;

	i = 0;
	while(i < Length)
	{
		u32 BufAt0 = read32((u32)Buffer+i);
		if( BufAt0 != 0x4E800020 )
		{
			if(PatchProcessorInterface(BufAt0, (u32)Buffer + i) || PatchTimers(BufAt0, (u32)Buffer + i, true))
			{
				i += 4;
				continue;
			}
			if(PatchWide && PatchWidescreen(BufAt0, (u32)Buffer+i))
			{
				PatchWide = false;
				i += 4;
				continue;
			}

			u32 BufLowAt0 = BufAt0 & 0xFFFF;
			u32 BufHighAt0 = BufAt0 >> 16;
			u32 BufAt4 = read32((u32)Buffer+i+4);

			if( (PatchCount & FPATCH_SetInterruptMask) == 0 )
			{
				if( BufAt0 == 0x5480056A && BufAt4 == 0x28000000 &&
					read32( (u32)Buffer + i + 8 ) == 0x40820008 &&
					(read32( (u32)Buffer + i + 12 ) & 0xFC00FFFF) == 0x60000004 )
				{
					printpatchfound("SetInterruptMask",NULL,  (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					if(write32A((u32)Buffer + i - 0x30, 0x60000000, 0x93E50028, 0)) // EXI Channel 2 Status Register
						printpatchfound("SetInterruptMask", "DBG", (u32)Buffer + i - 0x30);

					write32( (u32)Buffer + i + 12, (read32( (u32)Buffer + i + 12 ) & 0xFFFF0000) | 0x4004 );

					PatchCount |= FPATCH_SetInterruptMask;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufAt0 == 0x3D00CC00 && BufAt4 == 0x61083004 && read32( (u32)Buffer + i + 8 ) == 0x392000F0)
				{
					dbgprintf("Patch:[SetInterruptMask_Datel]: (0x%08X)\r\n", Buffer+i+0x8);
					write32( (u32)Buffer+i+0x8, 0x392040F0 );
					PatchCount |= FPATCH_SetInterruptMask;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufAt0 == 0x3D60CC00 && BufAt4 == 0x616B3004 && read32( (u32)Buffer + i + 0xC ) == 0x38000000
					 && read32( (u32)Buffer + i + 0x10 ) == 0x914B0000 && read32( (u32)Buffer + i - 0x4 ) == 0x91490000
					 && read32( (u32)Buffer + i - 0xC ) == 0x39400000)
				{
					dbgprintf("Patch:[SetInterruptMask_DatelB]: (0x%08X)\r\n", Buffer+i-0xC);
					write32( (u32)Buffer+i-0xC, 0x38000000 );
					write32( (u32)Buffer+i+0xC, 0x39404000 );
					write32( (u32)Buffer+i-0x4, 0x90090000 );
					PatchCount |= FPATCH_SetInterruptMask;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
			}
			if( (PatchCount & FPATCH_OSDispatchIntr) == 0 )
			{
				if( BufAt0 == 0x3C60CC00 &&
					BufAt4 == 0x83E33000)
				{
					printpatchfound("__OSDispatchInterrupt", NULL, (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					PatchBL( PatchCopy(FakeInterrupt, FakeInterrupt_size), (u32)Buffer + i + 4 );
					if(DisableEXIPatch == 0)
					{
						// EXI Device 0 Control Register
						write32A( (u32)Buffer+i+0x114, 0x3C60C000, 0x3C60CC00, 1 );
						write32A( (u32)Buffer+i+0x118, 0x80830010, 0x80836800, 1 );
						// EXI Device 1 Control Register
						write32A( (u32)Buffer+i+0x14C, 0x3C60C000, 0x3C60CC00, 1 );
						write32A( (u32)Buffer+i+0x150, 0x38630014, 0x38636800, 1 );
						write32A( (u32)Buffer+i+0x154, 0x80830000, 0x80830014, 1 );
						if(TRIGame)
						{
							// EXI Device 2 Control Register (Trifroce)
							write32A( (u32)Buffer+i+0x188, 0x3C60C000, 0x3C60CC00, 1 );
							write32A( (u32)Buffer+i+0x18C, 0x38630018, 0x38636800, 1 );
							write32A( (u32)Buffer+i+0x190, 0x80830000, 0x80830028, 1 );
						}
					}
					if (!Datel) // Multiples in Datel discs
						PatchCount |= FPATCH_OSDispatchIntr;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufAt0 == 0x3C60CC00 && BufAt4 == 0x83A33000 && read32( (u32)Buffer + i + 8 ) == 0x57BD041C)
				{
					printpatchfound("__OSDispatchInterrupt", "DBG", (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					PatchBL( PatchCopy(FakeInterrupt_DBG, FakeInterrupt_DBG_size), (u32)Buffer + i + 4 );
					if(DisableEXIPatch == 0)
					{
						// EXI Device 0 Control Register
						write32A( (u32)Buffer+i+0x138, 0x3C60C000, 0x3C60CC00, 1 );
						write32A( (u32)Buffer+i+0x13C, 0x83C30010, 0x83C36800, 1 );
					}
					PatchCount |= FPATCH_OSDispatchIntr;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufAt0 == 0x3D20CC00 && BufAt4 == 0x61293000 && read32( (u32)Buffer + i + 8 ) == 0x83890000)
				{
					printpatchfound("__OSDispatchInterrupt_Datel", NULL, (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					PatchBL( PatchCopy(FakeInterrupt_Datel, FakeInterrupt_Datel_size), (u32)Buffer + i + 8 );
					PatchCount |= FPATCH_OSDispatchIntr;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufAt0 == 0x3D20CC00 && BufAt4 == 0x61293000 && read32( (u32)Buffer + i + 8 ) == 0x81490000)
				{
					printpatchfound("__OSDispatchInterrupt_Datel", NULL, (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					PatchBL( PatchCopy(FakeInterrupt_Datel, FakeInterrupt_Datel_size), (u32)Buffer + i + 8 );
					PatchCount |= FPATCH_OSDispatchIntr;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
			}
			if( (PatchCount & FPATCH_DVDIntrHandler) == 0 )
			{
				if( BufLowAt0 == 0x6000 && (BufAt4 & 0xFFFF) == 0x002A &&
					(read32( (u32)Buffer + i + 8 ) & 0xFFFF) == 0x0054 )
				{
					u32 Offset = (u32)Buffer + i - 8;
					u32 HwOffset = Offset;
					if ((read32(Offset + 4) & 0xFFFF) == 0xD302)	// Loader
						HwOffset = Offset + 4;
					dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
					PatchBL(__DVDInterruptHandlerAddr, HwOffset);
					PatchCount |= FPATCH_DVDIntrHandler;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufLowAt0 == 0x6000 && 
					(read32( (u32)Buffer + i + 8 ) & 0xFFFF) == 0x002A &&
					(read32( (u32)Buffer + i + 0xC ) & 0xFFFF) == 0x0054 )
				{
					u32 Offset = (u32)Buffer + i - 8;
					u32 HwOffset = Offset;
					if ((read32(Offset + 4) & 0xFFFF) == 0xD302)	// Loader
						HwOffset = Offset + 4;
					dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
					u32 OrigData = read32(HwOffset);
					PatchBL(__DVDInterruptHandlerAddr, HwOffset);
					if (Datel)
					{
						write32(__DVDInterruptHandlerAddr + __DVDInterruptHandler_size - 0x8, OrigData);
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", OrigData, HwOffset);
						#endif
						u32 OrigAddr = Offset + 0x74;
						dbgprintf("Patch:[__DVDInterruptHandler]: (0x%08X)\r\n", read32(OrigAddr));
						if (write32A(OrigAddr, 0x7CC03378, 0x7CE03378, false)) //or r0,r7,r6 -> or r0,r6,r6
							printpatchfound("DVDInterruptHandler", "Datel", OrigAddr);
						OrigAddr = Offset - 0xC;
						dbgprintf("Patch:[__DVDInterruptHandler]: (0x%08X)\r\n", read32(OrigAddr));
						if (write32A(OrigAddr, 0x60000000, 0x3C00E000, false)) //or r0,r7,r6 -> or r0,r6,r6
							printpatchfound("DVDInterruptHandler", "Datel", OrigAddr);
					}
					PatchCount |= FPATCH_DVDIntrHandler;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufLowAt0 == 0x6000 && 
					(read32( (u32)Buffer + i + 0x1C ) & 0xFFFF) == 0x002A &&
					(read32( (u32)Buffer + i + 0x20 ) & 0xFFFF) == 0x0054 )
				{
					u32 Offset = (u32)Buffer + i - 8;
					u32 HwOffset = Offset;
					if ((read32(Offset + 4) & 0xFFFF) == 0xD302)	// Loader
						HwOffset = Offset + 4;
					else
					{
						dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
						// Save r0 (lr) before bl
						u32 OrigData = read32(HwOffset);
						write32(HwOffset, read32(HwOffset + 4));
						write32(HwOffset + 4, OrigData);
						HwOffset = Offset + 4;
					}
					dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
					u32 OrigData = read32(HwOffset);
					PatchBL(__DVDInterruptHandlerAddr, HwOffset);
					if (Datel)
					{
						write32(__DVDInterruptHandlerAddr + __DVDInterruptHandler_size - 0xC, OrigData);
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", OrigData, HwOffset);
						#endif
					}
					PatchCount |= FPATCH_DVDIntrHandler;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
			}
			if( (PatchCount & FPATCH_VIConfigure) == 0 )
			{
				if( BufAt0 == 0x3C608000 && (BufAt4 & 0xFC1FFFFF) == 0x800300CC && (read32( (u32)Buffer + i + 8 ) >> 24) == 0x54 )
				{
					printpatchfound("VIConfigure", NULL, (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					write32( (u32)Buffer + i + 4, 0x5400F0BE | ((read32( (u32)Buffer + i + 4 ) & 0x3E00000) >> 5) );
					PatchCount |= FPATCH_VIConfigure;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
			}
			if( (PatchCount & FPATCH_getTiming) == 0 )
			{
				if( BufAt0 == 0x386500BE && BufAt4 == 0x4E800020 && 
					read32((u32)Buffer+i+8) == 0x386500E4 && read32((u32)Buffer+i+0xC) == 0x4E800020 )
				{
					u32 PatchOffset = 0x10;
					while ((read32((u32)Buffer+i+PatchOffset) != 0x38600000) && (PatchOffset < 0x30)) // MaxSearch=0x20
						PatchOffset += 4;
					if(read32((u32)Buffer+i+PatchOffset) == 0x38600000 &&
						read32((u32)Buffer+i+PatchOffset+4) == 0x4E800020)
					{
						printpatchfound("getTiming", NULL, (u32)Buffer+i+PatchOffset);
						write32( (u32)Buffer+i+PatchOffset, 0x7CA32B78 ); //mr r5, r3
						PatchCount |= FPATCH_getTiming;
						i += PatchOffset+4;
						continue;
					}
				}
			}
			if( (PatchCount & FPATCH_GXInit) == 0 )
			{
				if( BufAt0 == 0x3C80CC00 ) /* Release SDK */
				{
					u32 p1 = 0;
					int j = i + 4;
					while(read32((u32)Buffer+j) != 0x4E800020)
					{
						if( !p1 && read32((u32)Buffer+j) == 0x38A43000 )
							p1 = 1;
						else if( p1 && R16((u32)Buffer+j) == 0x90AD )
						{
							piReg = R16( (u32)Buffer + j + 2 );
							dbgprintf("Patch:[GXInit] stw r5,-0x%X(r13) (0x%08X)\r\n", 0x10000 - piReg, (u32)Buffer + j + 2);
							PatchCount |= FPATCH_GXInit;
							i = GotoFuncEnd(j, (u32)Buffer);
							break;
						}
						j += 4;
					}
				}
				else if( BufAt0 == 0x3C600C00 ) /* Debug SDK */
				{
					u32 p1 = 0;
					int j = i + 4;
					while(read32((u32)Buffer+j) != 0x4E800020)
					{
						if( !p1 && read32((u32)Buffer+j) == 0x38633000 )
							p1 = 1;
						else if( p1 && R16((u32)Buffer+j) == 0x906D )
						{
							piReg = R16( (u32)Buffer + j + 2 );
							dbgprintf("Patch:[GXInit DBG] stw r3,-0x%X(r13) (0x%08X)\r\n", 0x10000 - piReg, (u32)Buffer + j + 2);
							PatchCount |= FPATCH_GXInit;
							i = GotoFuncEnd(j, (u32)Buffer);
							break;
						}
						j += 4;
					}
				}
			}
			if((PatchCount & FPATCH_CARDUnlock) == 0)
			{
				if(BufAt0 == 0x38C00008 && read32((u32)Buffer+i+16) == 0x38800008 && read32((u32)Buffer+i+28) == 0x90DD0004)
				{
					printpatchfound("__CARDUnlock", "A", (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					write32( (u32)Buffer+i, 0x3CC01000 ); //lis r6, 0x1000
					write32( (u32)Buffer+i+28, 0x909D0004 ); //stw r4, 4(r29)
					write32( (u32)Buffer+i+36, 0x90DD0008 ); //stw r6, 8(r29)
					PatchCount |= FPATCH_CARDUnlock;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if(BufAt0 == 0x38000008 && BufAt4 == 0x54A47820 && 
					read32((u32)Buffer+i+20) == 0x901F0044 && read32((u32)Buffer+i+28) == 0x7C801A78 && 
					read32((u32)Buffer+i+40) == 0x3B400000 && read32((u32)Buffer+i+60) == 0x38800008)
				{
					printpatchfound("__CARDUnlock", "IPL A", (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					write32( (u32)Buffer+i, 0x38800008 ); //li r4, 8
					write32( (u32)Buffer+i+4, 0x54A07820 ); //slwi r0, r5, 15
					write32( (u32)Buffer+i+20, 0x909F0044 ); //stw r4, 0x44(r31)
					write32( (u32)Buffer+i+28, 0x7C001A78 ); //xor r0, r0, r3
					write32( (u32)Buffer+i+40, 0x3F401000 ); //lis r26, 0x1000
					write32( (u32)Buffer+i+60, 0x3B400000 ); //li r26, 0
					PatchCount |= FPATCH_CARDUnlock;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if(BufAt0 == 0x38A00008 && read32((u32)Buffer+i+8) == 0x38000000 &&
					read32((u32)Buffer+i+44) == 0x90BD0004 && read32((u32)Buffer+i+52) == 0x901D0008)
				{
					printpatchfound("__CARDUnlock", "IPL B", (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					write32( (u32)Buffer+i+8, 0x3C001000 ); //lis r0, 0x1000
					PatchCount |= FPATCH_CARDUnlock;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if(BufAt0 == 0x38000008 && BufAt4 == 0x90180004 && 
					read32((u32)Buffer+i+16) == 0x38000000 && read32((u32)Buffer+i+20) == 0x90180008)
				{
					printpatchfound("__CARDUnlock", "DBG", (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					write32( (u32)Buffer+i+16, 0x3C001000 ); //lis r0, 0x1000
					PatchCount |= FPATCH_CARDUnlock;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
			}
			if( (PatchCount & FPATCH_OSSleepThread) == 0 )
			{
				//OSSleepThread(Pattern 1)
				if( BufAt0 == 0x3C808000 &&
					( BufAt4 == 0x3C808000 || BufAt4 == 0x808400E4 ) &&
					( read32((u32)Buffer + i + 8 ) == 0x38000004 || read32((u32)Buffer + i + 8 ) == 0x808400E4 ) )
				{
					PatchCount |= FPATCH_OSSleepThread;
					OSSleepThread = (u32)Buffer + GotoFuncStart(i, (u32)Buffer);
					printpatchfound("OSSleepThread",NULL,OSSleepThread);
					i = GotoFuncEnd(i, (u32)Buffer);
					OSSleepThreadHook = (u32)Buffer + i;
					printpatchfound("Hook:OSSleepThread",NULL,(u32)Buffer + i);
					OSWakeupThread = OSSleepThreadHook + 4;
					printpatchfound("OSWakeupThread",NULL,OSWakeupThread);
					continue;
				}
			}
	#ifdef CHEATS
			/*if( (PatchCount & FPATCH_GXBegin) == 0 )
			{
				//GXBegin(Pattern 1)
				if( BufAt0 == 0x3C60CC01 && BufAt4 == 0x98038000 &&
					( read32((u32)Buffer + i + 8 ) == 0xB3E38000 || read32((u32)Buffer + i + 8 ) == 0xB3C38000 ) )
				{
					PatchCount |= FPATCH_GXBegin;
					i = GotoFuncEnd(i, (u32)Buffer);
					if(DebuggerHook == 0) DebuggerHook = (u32)Buffer + i;
					else if(DebuggerHook2 == 0) DebuggerHook2 = (u32)Buffer + i;
					else if(DebuggerHook3 == 0) DebuggerHook3 = (u32)Buffer + i;
					printpatchfound("Hook:GXBegin",NULL,(u32)Buffer + i);
					continue;
				}
			}
			if( (PatchCount & FPATCH_GXDrawDone) == 0 )
			{
				//GXDrawDone(Pattern 1)
				if( BufAt0 == 0x3CC0CC01 && BufAt4 == 0x3CA04500 &&
					read32((u32)Buffer + i + 12) == 0x38050002 && read32((u32)Buffer + i + 16) == 0x90068000 )
				{
					i = GotoFuncEnd(i, (u32)Buffer);
					if(DebuggerHook == 0) DebuggerHook = (u32)Buffer + i;
					else if(DebuggerHook2 == 0) DebuggerHook2 = (u32)Buffer + i;
					else if(DebuggerHook3 == 0) DebuggerHook3 = (u32)Buffer + i;
					printpatchfound("Hook:GXDrawDone",NULL,(u32)Buffer + i);
					continue;
				} //GXDrawDone(Pattern 2)
				else if( BufAt0 == 0x3CA0CC01 && BufAt4 == 0x3C804500 &&
					read32((u32)Buffer + i + 12) == 0x38040002 && read32((u32)Buffer + i + 16) == 0x90058000 )
				{
					i = GotoFuncEnd(i, (u32)Buffer);
					if(DebuggerHook == 0) DebuggerHook = (u32)Buffer + i;
					else if(DebuggerHook2 == 0) DebuggerHook2 = (u32)Buffer + i;
					else if(DebuggerHook3 == 0) DebuggerHook3 = (u32)Buffer + i;
					printpatchfound("Hook:GXDrawDone",NULL,(u32)Buffer + i);
					continue;
				} //GXDrawDone(Pattern 3)
				else if( BufAt0 == 0x3FE04500 && BufAt4 == 0x3BFF0002 &&
					read32((u32)Buffer + i + 20) == 0x3C60CC01 && read32((u32)Buffer + i + 24) == 0x93E38000 )
				{
					i = GotoFuncEnd(i, (u32)Buffer);
					if(DebuggerHook == 0) DebuggerHook = (u32)Buffer + i;
					else if(DebuggerHook2 == 0) DebuggerHook2 = (u32)Buffer + i;
					else if(DebuggerHook3 == 0) DebuggerHook3 = (u32)Buffer + i;
					printpatchfound("Hook:GXDrawDone",NULL,(u32)Buffer + i);
					continue;
				}//GXDrawDone(Pattern 4)
				else if( BufAt0 == 0x3C804500 && read32((u32)Buffer + i + 12) == 0x3CA0CC01 &&
					read32((u32)Buffer + i + 28) == 0x38040002 && read32((u32)Buffer + i + 40) == 0x90058000 )
				{
					i = GotoFuncEnd(i, (u32)Buffer);
					if(DebuggerHook == 0) DebuggerHook = (u32)Buffer + i;
					else if(DebuggerHook2 == 0) DebuggerHook2 = (u32)Buffer + i;
					else if(DebuggerHook3 == 0) DebuggerHook3 = (u32)Buffer + i;
					printpatchfound("Hook:GXDrawDone",NULL,(u32)Buffer + i);
					continue;
				}
			}*/
	#endif
			if( (PatchCount & FPATCH_VideoModes) == 0 )
			{
				/* might be a bit overkill but should detect most video mode in memory */
				while((BufAt0 & 0xFFFFFF00) == 0 && (BufAt4 & 0xFF000000) == 0x02000000
					&& (read32((u32)Buffer+i+12) & 0xFF00FF00) == 0x00000200 && read32((u32)Buffer+i+24) == 0x00000606
					&& read32((u32)Buffer+i+32) == 0x06060606 && read32((u32)Buffer+i+44) == 0x06060606)
				{
					if(ConfigGetVideoMode() & NIN_VID_FORCE)
					{
						if(ConfigGetVideoMode() & NIN_VID_FORCE_DF)
						{
							if(memcmp(Buffer+i+0x32, GXDeflickerOff, sizeof(GXDeflickerOff)) == 0)
								memcpy(Buffer+i+0x32, GXDeflickerOn, sizeof(GXDeflickerOn));
						}
						else
						{
							if(memcmp(Buffer+i+0x32, GXDeflickerOn, sizeof(GXDeflickerOn)) == 0)
								memcpy(Buffer+i+0x32, GXDeflickerOff, sizeof(GXDeflickerOff));
						}
					}
					if( ConfigGetConfig(NIN_CFG_FORCE_PROG) )
					{
						switch(read32((u32)Buffer+i))
						{
							case 0x00: //NTSC
								printvidpatch(VI_NTSC, VI_480P, (u32)Buffer+i);
								write32( (u32)Buffer+i, 0x02 );
								write32( (u32)Buffer+i+0x14, 0); //mode sf
								break;
							case 0x04: //PAL50
								if(!(ConfigGetVideoMode() & NIN_VID_PATCH_PAL50))
									break;
								printvidpatch(VI_PAL, VI_480P, (u32)Buffer+i);
								write32( (u32)Buffer+i, 0x02 );
								//write32( (u32)Buffer+i, 0x06 );
								memcpy( Buffer+i+0x04, GXIntDfAt04, sizeof(GXIntDfAt04) ); //terrible workaround I know
								write32( (u32)Buffer+i+0x14, 0); //mode sf
								break;
							case 0x08: //MPAL
							case 0x14: //PAL60
								printvidpatch(VI_EUR60, VI_480P, (u32)Buffer+i);
								write32( (u32)Buffer+i, 0x02 );
								//write32( (u32)Buffer+i, 0x16 );
								write32( (u32)Buffer+i+0x14, 0); //mode sf
								break;
							default:
								break;
						}
					}
					else if( ConfigGetVideoMode() & NIN_VID_FORCE )
					{
						u8 NinForceMode = ConfigGetVideoMode() & NIN_VID_FORCE_MASK;
						switch(read32((u32)Buffer+i))
						{
							case 0x00: //NTSC
								if(NinForceMode == NIN_VID_FORCE_NTSC)
									break;
								if(NinForceMode == NIN_VID_FORCE_MPAL)
								{
									printvidpatch(VI_NTSC, VI_MPAL, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x08 );
								}
								else //PAL60
								{
									printvidpatch(VI_NTSC, VI_EUR60, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x14 );
								}
								write32( (u32)Buffer+i+0x14, 1); //mode df
								break;
							case 0x08: //MPAL
								if(NinForceMode == NIN_VID_FORCE_MPAL)
									break;
								if(NinForceMode == NIN_VID_FORCE_NTSC)
								{
									printvidpatch(VI_MPAL, VI_NTSC, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x00 );
								}
								else //PAL60
								{
									printvidpatch(VI_MPAL, VI_EUR60, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x14 );
								}
								write32( (u32)Buffer+i+0x14, 1); //mode df
								break;
							case 0x04: //PAL50
								if(NinForceMode == NIN_VID_FORCE_PAL50 || NinForceMode == NIN_VID_FORCE_PAL60
										|| !(ConfigGetVideoMode() & NIN_VID_PATCH_PAL50))
									break;
								if(NinForceMode == NIN_VID_FORCE_MPAL)
								{
									printvidpatch(VI_PAL, VI_MPAL, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x08 );
								}
								else //NTSC
								{
									printvidpatch(VI_PAL, VI_NTSC, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x00 );
								}
								memcpy( Buffer+i+0x04, GXIntDfAt04, sizeof(GXIntDfAt04) ); //terrible workaround I know
								write32( (u32)Buffer+i+0x14, 1); //mode df
								break;
							case 0x14: //PAL60
								if(NinForceMode == NIN_VID_FORCE_PAL50 || NinForceMode == NIN_VID_FORCE_PAL60)
									break;
								if(NinForceMode == NIN_VID_FORCE_MPAL)
								{
									printvidpatch(VI_EUR60, VI_MPAL, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x08 );
								}
								else //NTSC
								{
									printvidpatch(VI_EUR60, VI_NTSC, (u32)Buffer+i);
									write32( (u32)Buffer+i, 0x00 );
								}
								write32( (u32)Buffer+i+0x14, 1); //mode df
								break;
							default:
								break;
						}
					}
					if(ConfigGetVideoScale() >= 40 && ConfigGetVideoScale() <= 120)
					{
						W16((u32)Buffer+i+0xE, ConfigGetVideoScale() + 600);
						W16((u32)Buffer+i+0xA, (720 - R16((u32)Buffer+i+0xE)) / 2);
					}
					if(ConfigGetVideoOffset() >= -20 && ConfigGetVideoOffset() <= 20)
					{
						u16 xorig = R16((u32)Buffer+i+0xA);
						if((xorig + ConfigGetVideoOffset()) < 0)
							W16((u32)Buffer+i+0xA, 0);
						else if((xorig + ConfigGetVideoOffset()) > 80)
							W16((u32)Buffer+i+0xA, 80);
						else
							W16((u32)Buffer+i+0xA, xorig + ConfigGetVideoOffset());
					}
					i += 0x3C;
					continue;
				}
			}
			if (Datel)
			{
				if (BufAt0 == 0x7C0903A6 &&    // mtctr %r0
					BufAt4 == 0x42000000)      // bdnz 0 (itself)
				{
					u32 BufAtMinus4 = read32((u32)Buffer+i-4);
					if (BufAtMinus4 == 0x38006800) // li %r0, 0x6800
					{
						// 0x6800 * 3 / 2 = 0x9C00  is too big to fit in li %r0, imm
						// Use branch to function
						printpatchfound("Timer", "Datel", (u32)Buffer+i-4);
						PatchB(DatelTimerAddr, (u32)Buffer+i-4);  
						i += 8;
						continue;
					}
					if (BufAtMinus4 == 0x380001DB) // li %r0, 0x1DB
					{
						// 0x1DB * 3 /2 = 0x2C8.8
						printpatchfound("ShortTimer", "Datel", (u32)Buffer+i-4);
						write32((u32)Buffer+i-4, 0x380002C9); // li %r0, 0x2C9
						i += 8;
						continue;
					}
				}
			}
			if( (PatchCount & FPATCH_DSP_ROM) == 0 && ( BufHighAt0 == 0x29F || (BufAt4 >> 16) == 0x29F) )
			{
				vu32 l; //volatile so devkitARM r46 doesnt optimize it away
				s32 Known = -1;

				u32 UseLast = 0;
				bool PatternMatch = false;
				// for each pattern/length combo
				for( l=0; l < sizeof(DspMatches) / sizeof(DspMatch); ++l )
				{
					if (UseLast == 0)
						PatternMatch = ( memcmp( (void*)(Buffer+i), DSPPattern[DspMatches[l].Pattern], 0x10 ) == 0 );
					if (PatternMatch)
					{
						#ifdef DEBUG_DSP
						dbgprintf("Patch:Matching [DSPPattern] (0x%08X) v%u\r\n", (u32)Buffer + i, l );
						#endif

						if (DspMatches[l].Length-UseLast > 0)
							memcpy( (void*)0x12E80000+UseLast, (unsigned char*)Buffer+i+UseLast, DspMatches[l].Length-UseLast );
						
						if (DspMatches[l].Length != UseLast)
						{
							sha1( SHA1i, NULL, 0, 0, NULL );
							sha1( SHA1i, (void*)0x12E80000, DspMatches[l].Length, 2, hash );
						}

						if( memcmp( DSPHashes[DspMatches[l].SHA1], hash, 0x14 ) == 0 )
						{
							Known = DspMatches[l].SHA1;
	#ifdef DEBUG_DSP
							dbgprintf("DSP before Patch\r\n");
							hexdump((void*)(Buffer + i), DspMatches[l].Length);
	#endif
							DoDSPPatch(Buffer + i, Known);
	#ifdef DEBUG_DSP
							dbgprintf("DSP after Patch\r\n");
							hexdump((void*)(Buffer + i), DspMatches[l].Length);
	#endif
							dbgprintf("Patch:[DSP v%u] patched (0x%08X)\r\n", Known, Buffer + i );
							//PatchCount |= FPATCH_DSP_ROM; // yes, games can
							//have multiple DSPs, check out Smugglers Run
							i += DspMatches[l].Length;
							break;
						}
					}
					if ( (l < sizeof(DspMatches) / sizeof(DspMatch)) && (DspMatches[l].Pattern == DspMatches[l+1].Pattern) )
						UseLast = DspMatches[l].Length;
					else
						UseLast = 0;
				}
				if(Known != -1) continue; //i is already advanced
			}
			i += 4;
			continue;
		}
		i+=4;

		MPattern( (u8*)(Buffer+i), maxPatternSize, &curFunc );
		/* only deal with functions with potentially correct function sizes */
		if(curFunc.Length < minPatternSize || curFunc.Length > maxPatternSize)
			continue;
		//if ((((u32)Buffer + i) & 0x7FFFFFFF) == 0x00000000) //(FuncPrint)
		//	dbgprintf("FuncPattern: 0x%X, %d, %d, %d, %d, %d\r\n", 
		//	curFunc.Length, curFunc.Loads, curFunc.Stores, curFunc.FCalls, curFunc.Branch, curFunc.Moves);
		for(patitr = 0; patitr < CurFPatternsListLen; ++patitr)
		{
			FuncPattern *CurPatterns = CurFPatternsList[patitr].pat;
			u32 CurPatternsLen = CurFPatternsList[patitr].patlen;
			bool patfound = false;
			for( j=0; j < CurPatternsLen; ++j )
			{
				if( CurPatterns[j].Found ) //Skip already found patches
					continue;

				if( CPattern( &curFunc, &(CurPatterns[j]) ) )
				{
					u32 FOffset = (u32)Buffer + i;

					CurPatterns[j].Found = FOffset;
					//dbgprintf("Patch:[%s] found (0x%08X)\r\n", CurPatterns[j].Name, FOffset );

					switch( CurPatterns[j].PatchLength )
					{
						case FCODE_Return:	// __AI_set_stream_sample_rate
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							write32( FOffset, 0x4E800020 );
						} break;
						case FCODE_AIResetStreamCount:	// Audiostreaming hack
						{
							switch( TITLE_ID )
							{
								case 0x474544:	// Eternal Darkness
								break;
								default:
								{
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
									write32( FOffset + 0xB4, 0x60000000 );
									write32( FOffset + 0xC0, 0x60000000 );
								} break;
							}
						} break;
						case FCODE_GXInitTlutObj:	// GXInitTlutObj
						{
							if( read32( FOffset+0x34 ) == 0x5400023E || //A
								read32( FOffset+0x28 ) == 0x5004C00E || //B
								read32( FOffset+0x11C ) == 0x5400023E ) //DBG
							{
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								memcpy( (void*)FOffset, GXInitTlutObj, GXInitTlutObj_size );
							}
							else
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE_GXLoadTlut:
						{
							if(read32(FOffset + 0x40) == 0x801E0004)
							{
								if(TITLE_ID == 0x47424F) //Burnout
								{
									u32 GXLoadTlutAddr = PatchCopy(GXLoadTlut, GXLoadTlut_size);
									PatchBL(GXLoadTlutAddr, FOffset + 0x40);
									if(read32(FOffset + 0x6C) == 0x801E0004)
										PatchBL(GXLoadTlutAddr, FOffset + 0x6C);
									else if(read32(FOffset + 0x70) == 0x801E0004)
										PatchBL(GXLoadTlutAddr, FOffset + 0x70);
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								}
								else
									dbgprintf("Patch:[GXLoadTlut] skipped (0x%08X)\r\n", FOffset);
							}
							else
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE___ARHandler:
						{
							if(read32(FOffset + 0x44) == 0x280C0000)
							{
								if(GameNeedsHook() && useipl == 0)
								{
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x44);
									PatchBL(PatchCopy(__ARHandler, __ARHandler_size), FOffset + 0x44);
								}
								else
								{
									dbgprintf("Patch:[__ARHandler] skipped (0x%08X)\r\n", FOffset);
								}
							}
							else
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE___ARChecksize_A:
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x57C);
							write32(FOffset + 0x57C, 0x48000378); // b +0x0378, No Expansion
						} break;
						case FCODE___ARChecksize_B:
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x370);
							write32(FOffset + 0x370, 0x48001464); // b +0x1464, No Expansion
						} break;
						case FCODE___ARChecksize_DBG_B:
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x15C);
							write32(FOffset + 0x15C, 0x48000188); // b +0x0188, No Expansion
						} break;
						case FCODE___ARChecksize_C:
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x484);
							write32(FOffset + 0x484, 0x60000000); // nop, Memory Present
							write32(FOffset + 0x768, 0x60000000); // nop, 16MB Internal
							write32(FOffset + 0xB88, 0x48000324); // b +0x0324, No Expansion
						} break;
						case FCODE___ARChecksize_DBG_A:
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x160);
							write32(FOffset + 0x160, 0x4800001C); // b +0x001C, Memory Present
							write32(FOffset + 0x2BC, 0x60000000); // nop, 16MB Internal
							write32(FOffset + 0x394, 0x4800017C); // b +0x017C, No Expansion
						} break;
	// Widescreen hack by Extrems
						case FCODE_C_MTXPerspective:	//	C_MTXPerspective
						{
							if( !PatchWide )
								break;
							if(read32(FOffset+0x20) != 0xFFA01090)
							{
								CurPatterns[j].Found = 0;
								break;
							}
							MTXPerspectiveOffset = FOffset;
						} break;
						case FCODE_C_MTXLightPerspective:	//	C_MTXLightPerspective
						{
							if( !PatchWide )
								break;
							if(read32(FOffset+0x24) != 0xFF601090)
							{
								CurPatterns[j].Found = 0;
								break;
							}
							MTXLightPerspectiveOffset = FOffset;
						} break;
						case FCODE_J3DUClipper_clip:	//	clip
						{
							if( !PatchWide )
								break;

							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							write32( FOffset,   0x38600000 );
							write32( FOffset+4, 0x4E800020 );
						} break;
						case FCODE___OSReadROM:	//	__OSReadROM
						{
							if(read32(FOffset+0x80) == 0x38A00004 ||
								read32(FOffset+0xA8) == 0x38A00004)
							{
								memcpy((void*)FOffset, ReadROM, ReadROM_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE___OSInitAudioSystem_A:
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x4C);
							write32( FOffset+0x4C, 0x48000060 ); // b +0x0060, Skip Checks
						} break;
						case FCODE___OSInitAudioSystem_B:
						{
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x60);
							write32( FOffset+0x60, 0x48000060 ); // b +0x0060, Skip Checks
						} break;
						case FCODE_OSExceptionInit:
						case FCODE_OSExceptionInit_DBG:
						{
							if(cheatsWanted || debuggerWanted)
							{
								u32 patchOffset = (CurPatterns[j].PatchLength == FCODE_OSExceptionInit ? 0x1D4 : 0x1F0);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + patchOffset);
								u32 OSExceptionInitAddr = PatchCopy(OSExceptionInit, OSExceptionInit_size);
								write32(OSExceptionInitAddr, read32(FOffset + patchOffset)); //original instruction
								PatchBL(OSExceptionInitAddr, FOffset + patchOffset); //jump to comparison
							}
							else
								dbgprintf("Patch:[OSExceptionInit] skipped (0x%08X)\r\n", FOffset);
						} break;
						case FCODE___GXSetVAT:
						{
							if((GAME_ID) == 0x475A4D50) //dont know why needed
							{
								dbgprintf("Patch:[__GXSetVAT] skipped (0x%08X)\r\n", FOffset);
								break;
							}
							u32 BaseReg = 0, L1 = 0, L2 = 0, L3 = 0, RegLoc = 0;
							if((read32(FOffset) & 0xFE000000) == 0x80000000)
							{
								if(CurPatterns[j].Length == 0x98 &&
									(read32(FOffset+0x94) & 0xFE000000) == 0x98000000)
								{	//Pattern A
									BaseReg = read32(FOffset) & 0x1FFFFF;
									L1 = read32(FOffset+0x38) & 0xFFFF;
									L2 = read32(FOffset+0x44) & 0xFFFF;
									L3 = read32(FOffset+0x50) & 0xFFFF;
									RegLoc = read32(FOffset+0x94) & 0xFFFF;
								}
								else if(CurPatterns[j].Length == 0x84 && 
									(read32(FOffset+0x80) & 0xFE000000) == 0x98000000)
								{	//Pattern B
									BaseReg = read32(FOffset) & 0x1FFFFF;
									L1 = read32(FOffset+0x28) & 0xFFFF;
									L2 = read32(FOffset+0x34) & 0xFFFF;
									L3 = read32(FOffset+0x40) & 0xFFFF;
									RegLoc = read32(FOffset+0x80) & 0xFFFF;
								}
							}
							if(BaseReg)
							{
								memcpy( (void*)FOffset, __GXSetVAT, __GXSetVAT_size );
								write32(FOffset, (read32(FOffset) & 0xFFE00000) | BaseReg);
								write32(FOffset+0xC, (read32(FOffset+0xC) & 0xFFFF0000) | RegLoc);
								write32(FOffset+0x2C, (read32(FOffset+0x2C) & 0xFFFF0000) | L1);
								write32(FOffset+0x40, (read32(FOffset+0x40) & 0xFFFF0000) | L2);
								write32(FOffset+0x54, (read32(FOffset+0x54) & 0xFFFF0000) | L3);
								write32(FOffset+0x74, (read32(FOffset+0x74) & 0xFFFF0000) | RegLoc);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_EXIIntrruptHandler:
						{
							u32 PatchOffset = 0x4;
							while ((read32(FOffset + PatchOffset) != 0x90010004) && (PatchOffset < 0x20)) // MaxSearch=0x20
								PatchOffset += 4;
							if (read32(FOffset + PatchOffset) != 0x90010004) 	// stw r0, 0x0004(sp)
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							PatchBL( PatchCopy(TCIntrruptHandler, TCIntrruptHandler_size), (FOffset + PatchOffset) );
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + PatchOffset);
						} break;
						case FCODE_EXIDMA:	// EXIDMA
						{
							u32 off		= 0;
							s32 reg		=-1;
							u32 valueB	= 0;
							u32 ChanLen 	= 0x38;

							while(1)
							{
								off += 4;

								u32 op = read32( (u32)FOffset + off );
								// Look for rlwinm rX,rY,6 or invert logic and look for multi rX,rY,0x38 (0x1C000038)?
								if ((op & 0xFC00FFFF) == 0x54003032) //rlwinm rX, rY, 6,0,25
									ChanLen = 0x40;

								if( reg == -1 )
								{
									if( (op & 0xFC1F0000) == 0x3C000000 )	// lis rX, 0xCC00
									{
										reg = (op & 0x3E00000) >> 21;
										if( reg == 3 )
										{
											value = read32( FOffset + off ) & 0x0000FFFF;
											//dbgprintf("lis:%08X value:%04X\r\n", FOffset+off, value );
										}
									}
								}
								if( reg != -1 )
								{
									if( (op & 0xFC000000) == 0x38000000 )	// addi rX, rY, z
									{
										u32 src = (op >> 16) & 0x1F;
										if( src == reg )
										{
											u32 dst = (op >> 21) & 0x1F;
											if(dst == 0)
											{
												valueB = read32( FOffset + off ) & 0x0000FFFF;
												//dbgprintf("addi:%08X value:%04X\r\n", FOffset+off, valueB);
												break;
											}
											else //false hit
											{
												value = 0;
												reg = -1;
											}
										}
									}
								}
								if( op == 0x4E800020 )	// blr
									break;
							}
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							memcpy( (void*)(FOffset), EXIDMA, sizeof(EXIDMA) );
							/* Insert CB value into patched EXIDMA */
							W16(FOffset + 2, value);
							W16(FOffset + 6, valueB);
							dbgprintf("Patch:[EXIDMA] ChanLen:%08X\r\n", ChanLen);
							W16(FOffset + 0x12, ChanLen);
						} break;
						case FCODE_EXIUnlock:
						{
							u32 k;
							u32 found = 0;
							for(k = 0; k < CurPatterns[j].Length; k+=4)
							{
								if(read32(FOffset+k) == 0x54000734)
								{
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + k);
									memcpy((void*)FOffset, EXILock, sizeof(EXILock));
									found = 1;
									break;
								}
							}
							if(found == 0)
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE___CARDStat_A:	// CARD timeout
						{
							write32( FOffset+0x124, 0x60000000 );
							write32( FOffset+0x18C, 0x60000000 );
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE___CARDStat_B:	// CARD timeout
						{
							write32( FOffset+0x118, 0x60000000 );
							write32( FOffset+0x180, 0x60000000 );
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE___CARDStat_C:	// CARD timeout
						{
							write32( FOffset+0x124, 0x60000000 );
							write32( FOffset+0x194, 0x60000000 );
							write32( FOffset+0x1FC, 0x60000000 );
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE_ReadROM:
						{
							if(read32(FOffset+0x30) == 0x3BE00100 || 
								read32(FOffset+0x3C) == 0x3BE00100 || 
								read32(FOffset+0x44) == 0x38800100)
							{
								memcpy((void*)FOffset, ReadROM, ReadROM_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_ARInit:
						{
							/* Use GC Bus Clock Speed for Refresh Value */
							if(read32(FOffset + 0x5C) == 0x3C608000)
							{
								write32(FOffset + 0x5C, 0x3C6009A8);
								write32(FOffset + 0x64, 0x3803EC80);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x5C);
							}
							else if(read32(FOffset + 0x78) == 0x3C608000)
							{
								write32(FOffset + 0x78, 0x3C6009A8);
								write32(FOffset + 0x7C, 0x3803EC80);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x78);
							}
							else if(read32(FOffset + 0x24) == 0x38604000 || read32(FOffset + 0x2C) == 0x38604000)
							{
								dbgprintf("Patch:[ARInit] skipped (0x%08X)\r\n", FOffset);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_ARStartDMA:	//	ARStartDMA
						{
							if(useipl == 1)
							{
								memcpy( (void*)FOffset, ARStartDMA, ARStartDMA_size );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								break;
							}
							if(GameNeedsHook())
							{
								u32 PatchOffset = 0x20;
								while ((read32(FOffset + PatchOffset) != 0x3CC0CC00) && (PatchOffset < 0x40)) // MaxSearch=0x40
									PatchOffset += 4;
								if (read32(FOffset + PatchOffset) != 0x3CC0CC00)	// lis	r6,	0xCC00
								{
									CurPatterns[j].Found = 0; // False hit
									break;
								}
								PatchBL( PatchCopy(ARStartDMA_Hook, ARStartDMA_Hook_size), (FOffset + PatchOffset) );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + PatchOffset);
								break;
							}
							else if( (TITLE_ID) == 0x47384D ||	// Paper Mario
									 (TITLE_ID) == 0x474154 ||	// ATV Quad Power Racing 2
									 (TITLE_ID) == 0x47504E ||	// P.N.03
									 (TITLE_ID) == 0x474D4F ||	// Micro Machines
									 (TITLE_ID) == 0x475355 ||	// Superman: Shadow of Apokolips
									 (TITLE_ID) == 0x474859 ||	// Disney's The Haunted Mansion
									 DemoNeedsPaperMarioDMA())
							{
								memcpy( (void*)FOffset, ARStartDMA_PM, ARStartDMA_PM_size );
							}
							else if( (TITLE_ID) == 0x474759 ||	// Tom Clancy's Ghost Recon 2
									 (TITLE_ID) == 0x473633 )	// Tom Clancy's Rainbow Six 3
							{
								memcpy( (void*)FOffset, ARStartDMA_TC, ARStartDMA_TC_size );
							}
							else
							{
								memcpy( (void*)FOffset, ARStartDMA, ARStartDMA_size );
							}
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE_DVDSendCMDEncrypted:
						{
							if(read32(FOffset + 0x90) == 0x90E30008 && read32(FOffset + 0x94) == 0x90C3000C
								&& read32(FOffset + 0x98) == 0x90A30010)
							{
								/* Replace it with unencrypted version to look like normal GC DI */
								memcpy((void*)FOffset, DVDSendCMDEncrypted, DVDSendCMDEncrypted_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE_IsTriforceType3:
						{
							if(read32(FOffset + 0x290) == 0x5460063F && read32(FOffset + 0x2C8) == 0x2C030000)
							{
								/* Fake 0x29 DI CMD version to prevent freezes on newer triforce games */
								write32( FOffset + 0x290, 0x2C040000 ); //fake compare result to continue
								write32( FOffset + 0x2C4, 0x38600000 ); //dont change type, just return ok
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE_GCAMSendCommand:
						{
							if(read32(FOffset + 0x2C) == 0x38C40080)
							{
								u32 GCAMSendCommandAddr = PatchCopy(GCAMSendCommand, sizeof(GCAMSendCommand));
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								/* Data I/O */
								W16(GCAMSendCommandAddr+0x0E, R16(FOffset+0x1E));
								W16(GCAMSendCommandAddr+0x12, R16(FOffset+0x22));
								/* Callback */
								W16(GCAMSendCommandAddr+0x92, R16(FOffset+0x26));
								W16(GCAMSendCommandAddr+0x96, R16(FOffset+0x2A));
								PatchB(GCAMSendCommandAddr, FOffset);
							}
							else
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE___fwrite:	// patch_fwrite_Log
						case FCODE___fwrite_D:	// patch_fwrite_LogB
						{
							if( ConfigGetConfig( NIN_CFG_DEBUGGER ) || !ConfigGetConfig(NIN_CFG_OSREPORT) )
							{
								dbgprintf("Patch:[patch_fwrite_Log] skipped (0x%08X)\r\n", FOffset);
								break;
							}

							if( IsWiiU() )
							{
								if( CurPatterns[j].Patch == patch_fwrite_GC ) // patch_fwrite_Log works fine
								{
									dbgprintf("Patch:[patch_fwrite_GC] skipped (0x%08X)\r\n", FOffset);
									break;
								}
							}
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							memcpy((void*)FOffset, patch_fwrite_Log, sizeof(patch_fwrite_Log));
							if (CurPatterns[j].PatchLength == FCODE___fwrite_D)
							{
								write32(FOffset, 0x7C852379); // mr.     %r5,%r4
								write32(FOffset + sizeof(patch_fwrite_Log) - 0x8, 0x38600000); // li      %r3,0
							}
							break;
						}
						case FCODE__SITransfer:	//	_SITransfer
						{
							//e.g. Mario Strikers
							if (write32A(FOffset + 0x60, 0x7CE70078, 0x7CE70038, 0)) // clear errors - andc r7,r7,r0
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x60);

							//e.g. Billy Hatcher
							if (write32A(FOffset + 0xF8, 0x7F390078, 0x7F390038, 0)) // clear errors - andc r7,r7,r0
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0xF8);

							//e.g. Mario Strikers
							if (write32A(FOffset + 0x148, 0x5400007E, 0x50803E30, 0)) // clear tc - rlwinm r0,r0,0,1,31
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x148);

							//e.g. Luigi's Mansion
							if (write32A(FOffset + 0x140, 0x5400007E, 0x50A03E30, 0)) // clear tc - rlwinm r0,r0,0,1,31
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x140);

							//e.g. Billy Hatcher
							if (write32A(FOffset + 0x168, 0x5400007E, 0x50603E30, 0)) // clear tc - rlwinm r0,r0,0,1,31
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x168);

							//e.g. Batman Vengeance
							if (write32A(FOffset + 0x164, 0x5400007E, 0x50603E30, 0)) // clear tc - rlwinm r0,r0,0,1,31
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x164);
						} break;
						case FCODE_CompleteTransfer:	//	CompleteTransfer
						{
							//e.g. Mario Strikers
							if (write32A(FOffset + 0x38, 0x5400007C, 0x5400003C, 0)) // clear  tc - rlwinm r0,r0,0,1,30
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x38);

							//e.g. Billy Hatcher (func before CompleteTransfer does this)
							if (write32A(FOffset - 0x18, 0x57FF007C, 0x57FF003C, 0)) // clear  tc - rlwinm r31,r31,0,1,30
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset - 0x18);

							u32 k = 0;
							for(k = 0x10; k < 0x20; k+=4)
							{
								if (write32A(FOffset + k, 0x3C000000, 0x3C008000, 0)) // clear  tc - lis r0,0
								{
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + k);
									break;
								}
							}
						} break;
						case FCODE_SIInit:	//	SIInit
						{
							u32 k;
							u32 found = 0;
							for(k = 0x40; k < CurPatterns[j].Length; k += 4)
							{
								if (write32A(FOffset + k, 0x3C000000, 0x3C008000, 0)) // clear tc - lis r0,0
								{
									found = 1;
									SIInitOffset = FOffset + CurPatterns[j].Length;
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + k);
									/* Found SIInit, also sets SIInterruptHandler */
									u32 SIBase = 0;
									for(k += 4; k < CurPatterns[j].Length; k += 4)
									{
										if((read32(FOffset+k) & 0xFC00F000) == 0x3C008000)
											SIBase = (R16(FOffset+k+2) << 16);
										if(SIBase && (read32(FOffset+k) & 0xFFF00000) == 0x38800000)
										{
											SIBase = P2C(SIBase + (s16)R16(FOffset+k+2));
											u32 PatchOffset = 0x4;
											while ((read32(SIBase + PatchOffset) != 0x90010004) && (PatchOffset < 0x20)) // MaxSearch=0x20
												PatchOffset += 4;
											if (read32(SIBase + PatchOffset) != 0x90010004) 	// stw r0, 0x0004(sp)
												break;
											PatchBL( PatchCopy(SIIntrruptHandler, SIIntrruptHandler_size), (SIBase + PatchOffset) );
											printpatchfound("SIInterruptHandler", NULL, SIBase + PatchOffset);
											if (write32A(SIBase + 0xB4, 0x7F7B0078, 0x7F7B0038, 0)) // clear  tc - andc r27,r27,r0
												printpatchfound("SIInterruptHandler", NULL, SIBase + 0xB4);
											if (write32A(SIBase + 0x134, 0x7CA50078, 0x7CA50038, 0)) // clear  tc - andc r5,r5,r0
												printpatchfound("SIInterruptHandler", NULL, SIBase + 0x134);
											break;
										}
									}
									break;
								}
							}
							if(found == 0)
								CurPatterns[j].Found = 0; // False hit
						} break;
						case FCODE_SIPollingInterrupt:	//	SIEnablePollingInterrupt
						{
							//e.g. SSBM
							if (write32A(FOffset + 0x68, 0x60000000, 0x54A50146, 0)) // leave rdstint alone - nop
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x68);
							if (write32A(FOffset + 0x6C, 0x60000000, 0x54A5007C, 0)) // leave tcinit tcstart alone - nop
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x6C);

							//e.g. Billy Hatcher
							if (write32A(FOffset + 0x78, 0x60000000, 0x57FF0146, 0)) // leave rdstint alone - nop
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x78);
							if (write32A(FOffset + 0x7C, 0x60000000, 0x57FF007C, 0)) // leave tcinit tcstart alone - nop
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x7C);
						} break;
						case FCODE_PADRead:
						{
							if(DisableSIPatch)
							{
								dbgprintf("Patch:[PADRead] skipped (0x%08X)\r\n", FOffset);
								PADHook = FOffset + CurPatterns[j].Length;
							}
							else
							{
								memcpy((void*)FOffset, PADRead, PADRead_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								/* Search for PADInit over PADRead */
								u32 k;
								u32 lastEnd = 4;
								for(k = 8; k < 0x400; k += 4)
								{
									if(read32(FOffset - k) == 0x4E800020)
										lastEnd = k;
									else if((read32(FOffset - k) & 0xFC00FFFF) == 0x3C00F000)
									{
										PADInitOffset = FOffset - lastEnd;
										break;
									}
								}
							}
						} break;
						case FCODE___PADSetSamplingRate:
						{
							if(read32(FOffset+0x60) != 0x40800014 || read32(FOffset+0x8C) != 0xA003206C)
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							if(videoPatches)
							{
								write32(FOffset+0x60, 0x60000000); //anti-crash
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else
								dbgprintf("Patch:[__PADSetSamplingRate] skipped (0x%08X)\r\n", FOffset);
						} break;
						case FCODE_PADControlAllMotors:
						{
							u32 CheckOffset = CurPatterns[j].Length - 0x24;
							if((read32(FOffset+CheckOffset) & 0xFC00FFFF) != 0x2C000000) //cmpwi rX, 0
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							if(DisableSIPatch)
								dbgprintf("Patch:[PADControlAllMotors] skipped (0x%08X)\r\n", FOffset);
							else
							{
								memcpy((void*)FOffset, PADControlAllMotors, PADControlAllMotors_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
						} break;
						case FCODE_PADControlMotor:
						{
							if((read32(FOffset+0x24) & 0xFC00FFFF) != 0x3C008000) //lis rX, 0x8000
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							if(DisableSIPatch)
								dbgprintf("Patch:[PADControlMotor] skipped (0x%08X)\r\n", FOffset);
							else
							{
								memcpy((void*)FOffset, PADControlMotor, PADControlMotor_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
						} break;
						case FCODE_PADIsBarrel:
						{
							if(memcmp((void*)(FOffset), PADIsBarrelOri, sizeof(PADIsBarrelOri)) != 0)
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							if(DisableSIPatch)
								dbgprintf("Patch:[PADIsBarrel] skipped (0x%08X)\r\n", FOffset);
							else
							{
								memcpy((void*)(FOffset), PADIsBarrel, PADIsBarrel_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
						} break;
						case FCODE___OSResetHandler:
						{
							if(read32(FOffset + 0x84) == 0x540003DF)
							{
								if(DisableSIPatch)
									dbgprintf("Patch:[__OSResetSWInterruptHandler] skipped (0x%08X)\r\n", FOffset);
								else
								{
									PatchBL( FakeRSWLoadAddr, (FOffset + 0x84) );
									PatchBL( FakeRSWLoadAddr, (FOffset + 0x94) );
									PatchBL( FakeRSWStoreAddr, (FOffset + 0xD4) );
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x84);
								}
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_OSGetResetState:
						case FCODE_OSGetResetState_C:
						{
							if(DisableSIPatch)
								dbgprintf("Patch:[OSGetResetButtonState] skipped (0x%08X)\r\n", FOffset);
							else
							{
								if(CurPatterns[j].PatchLength == FCODE_OSGetResetState_C)
									PatchBL( FakeRSWLoadAddr, (FOffset + 0x28) );
								else /* Patch A, B */
									PatchBL( FakeRSWLoadAddr, (FOffset + 0x2C) );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
						} break;
						case FCODE___DSPHandler:
						{
							if(useipl == 1) break;
							if(read32(FOffset + 0xF8) == 0x2C000000)
							{
								if(DSPHandlerNeeded)
								{
									PatchBL( PatchCopy(__DSPHandler, __DSPHandler_size), (FOffset + 0xF8) );
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								}
								else
								{
									dbgprintf("Patch:[__DSPHandler] skipped (0x%08X)\r\n", FOffset);
								}
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE___DSPHandler_DBG:
						{
							if(useipl == 1) break;
							if(read32(FOffset + 0x14C) == 0x2C000000)
							{
								if(DSPHandlerNeeded)
								{
									PatchBL( PatchCopy(__DSPHandler, __DSPHandler_size), (FOffset + 0x14C) );
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								}
								else
								{
									dbgprintf("Patch:[__DSPHandler] skipped (0x%08X)\r\n", FOffset);
								}
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_PatchPatchBuffer:
						{
							PatchPatchBuffer( (char*)FOffset );
						} break;
						case FCODE_PrsLoad:
						{
							// HACK: PSO patch prs after dol conversion
							u32 OrigAddr = FOffset + 0x1A0;
							u32 Orig = read32(OrigAddr);
							if (Orig == 0x480000CD)
							{
								u32 BaseAddr = (Orig & 0x3FFFFFC);
								if (BaseAddr & 0x2000000) BaseAddr |= 0xFC000000;
								u32 NewAddr = (((s32)BaseAddr) + OrigAddr) | 0x80000000;
								dbgprintf("PSO:Extract Addr:0x%08X\r\n", NewAddr);
								write32(PRS_EXTRACT, NewAddr);
								sync_after_write((void*)PRS_EXTRACT, 0x20);
								printpatchfound("SwitcherPrs", NULL, OrigAddr);
								PatchBL(PatchCopy(SwitcherPrs, SwitcherPrs_size), OrigAddr);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_PsoDolEntryMod:
						{
							// HACK: PSO patch To Fake Entry
							u32 OrigAddr = FOffset + ((CurPatterns[j].Length == 0xC0) ? 0xAC : 0x23C);
							if ((read32(OrigAddr - 8) == 0x4C00012C) && (read32(OrigAddr) == 0x4E800021))  // isync and blrl
							{
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, OrigAddr);
								PatchBL(PATCH_OFFSET_ENTRY, OrigAddr);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DolEntryMod:
						{
							if (read32(FOffset + 0x38) == 0x4E800421) // bctrl
							{
								// HACK: Datel patch To Fake Entry
								PatchBL(PATCH_OFFSET_ENTRY, FOffset + 0x38);
								AppLoaderSize = read32(FOffset + 0x20);
								if ((AppLoaderSize & 0xFFFF0000) == 0x38800000)
									AppLoaderSize &= 0x0000FFFF;
								else
									AppLoaderSize = 0;
								printpatchfound("Datel", "FakeEntry", FOffset + 0x38);
								printpatchfound("Datel", "FakeEntrySize", AppLoaderSize);
							}
							else if (((u32)Buffer == 0x01300000) && (read32(FOffset + 0xA0) == 0x4C00012C) && (read32(FOffset + 0xC8) == 0x4E800021))  // isync and blrl
							{
								printpatchfound("Datel", "FakeEntry", FOffset + 0xC8);
								PatchBL(PATCH_OFFSET_ENTRY, FOffset + 0xC8);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_KeyboardRead:
						{
							if(DisableSIPatch)
								dbgprintf("Patch:[KeyboardRead] skipped (0x%08X)\r\n", FOffset);
							else
							{
								memcpy((void*)FOffset, KeyboardRead, KeyboardRead_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
						} break;
						case FCODE_SOInit:
						{
							if(read32(FOffset + 0x18) == 0x38000001 && read32(FOffset + 0x5C) == 0x38000001)
							{
								memcpy((void*)FOffset, SOInit, SOInit_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_SOStartup:
						{
							if(read32(FOffset+0x40C) == 0x38000001)
							{
								SOStartedOffset = read16(FOffset+0x412); //game startup status offset
								memcpy((void*)FOffset, SOStartup, SOStartup_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else if(read32(FOffset+0x3E8) == 0x38000001)
							{
								SOStartedOffset = read16(FOffset+0x3EE); //game startup status offset
								memcpy((void*)FOffset, SOStartup, SOStartup_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPGetMacAddr:
						{
							if(read32(FOffset + 0x28) == 0x4800000C && read32(FOffset + 0x40) == 0x389F0038)
							{
								memcpy((void*)FOffset, IPGetMacAddr, IPGetMacAddr_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPGetNetmask:
						{
							if(read32(FOffset + 0x28) == 0x4800000C && read32(FOffset + 0x40) == 0x389F0048)
							{
								memcpy((void*)FOffset, IPGetNetmask, IPGetNetmask_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPGetAddr:
						{
							if(read32(FOffset + 0x28) == 0x4800000C && read32(FOffset + 0x40) == 0x389F0044)
							{
								memcpy((void*)FOffset, IPGetAddr, IPGetAddr_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPGetAlias:
						{
							if(read32(FOffset + 0x28) == 0x4800000C && read32(FOffset + 0x40) == 0x389F0054)
							{
								//use same patch as for IPGetAddr
								memcpy((void*)FOffset, IPGetAddr, IPGetAddr_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPGetMtu:
						{
							if(read32(FOffset + 0x24) == 0x4800000C && read32(FOffset + 0x34) == 0x801F0040)
							{
								memcpy((void*)FOffset, IPGetMtu, IPGetMtu_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPGetLinkState:
						{
							if(read32(FOffset + 0x24) == 0x4800000C && read32(FOffset + 0x34) == 0x801F0004)
							{
								memcpy((void*)FOffset, IPGetLinkState, IPGetLinkState_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPGetConfigError:
						{
							if(read32(FOffset) == 0x28030000 && read32(FOffset + 8) == 0x4800000C
								&& read32(FOffset + 0x14) == 0x80630008)
							{
								memcpy((void*)FOffset, IPGetConfigError, IPGetConfigError_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPSetConfigError:
						{
							if(read32(FOffset + 0x24) == 0x4800000C && read32(FOffset + 0x34) == 0x801F0008
								&& read32(FOffset + 0x40) == 0x93DF0008)
							{
								memcpy((void*)FOffset, IPSetConfigError, IPSetConfigError_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_IPClearConfigError:
						{
							if(read32(FOffset + 0x20) == 0x4800000C && read32(FOffset + 0x30) == 0x83DF0008
								&& read32(FOffset + 0x38) == 0x901F0008)
							{
								memcpy((void*)FOffset, IPClearConfigError, IPClearConfigError_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_OSCreateThread:
						{
							if((read32(FOffset + 0xC8) == 0x3C808000 && read32(FOffset + 0xCC) == 0x38A400DC) || //pattern A
								(read32(FOffset + 0x19C) == 0x3C808000 && read32(FOffset + 0x1A0) == 0x38A400DC)) //pattern B
							{
								OSCreateThread = FOffset;
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_OSResumeThread:
						{
							if(read32(FOffset + 0x38) == 0x2C000000 && read32(FOffset + 0x54) == 0x2C000004)
							{
								OSResumeThread = FOffset;
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_avetcp_term:
						{
							if(read32(FOffset + 0x40) == 0x38600000 || //older lib
								read32(FOffset + 0x44) == 0x38600000) //newer lib
							{
								memcpy((void*)FOffset, avetcp_term, avetcp_term_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_ppp_term:
						{
							if((read32(FOffset + 0x2C) == 0x38600000) || //pattern A
								(read32(FOffset + 0x24) == 0x2C000000 && read32(FOffset + 0x30) == 0x2C000000 &&
								read32(FOffset + 0x64) == 0x38600000)) //pattern B
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_dix_term:
						{
							if(read32(FOffset + 0x30) == 0x2C000000 && read32(FOffset + 0x60) == 0x38000000)
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_some_unk_term:
						{
							if(read32(FOffset + 0x18) == 0xA87F0000 && read32(FOffset + 0x20) == 0x3800FFFF &&
								read32(FOffset + 0x24) == 0xB01F0000 && read32(FOffset + 0x28) == 0x7FE3FB78)
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_dns_clear_server:
						{
							if(read32(FOffset + 0x20) == 0x38800000 && read32(FOffset + 0x28) == 0x3BC00000)
							{
								memcpy((void*)FOffset, dns_clear_server, dns_clear_server_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_dns_close:
						{
							if(read32(FOffset + 0xC) == 0x7C600735 && read32(FOffset + 0x34) == 0x1C03003C &&
								read32(FOffset + 0x4C) == 0x3860FFFE)
							{
								memcpy((void*)FOffset, dns_close, dns_close_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_dns_term:
						{
							if((read32(FOffset + 0x48) == 0x2C000000 && read32(FOffset + 0x68) == 0x7C030000) || //older lib
								(read32(FOffset + 0x50) == 0x2C000000 && read32(FOffset + 0x70) == 0x7C030000)) //newer lib
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DHCP_request:
						{
							//since DHCP_request_nb gets patched just return its result immediately
							if(read32(FOffset + 0x14) == 0x2C030000 && read32(FOffset + 0x38) == 0x28000001 &&
								read32(FOffset + 0x58) == 0x3860FFFF && read32(FOffset + 0x60) == 0x38600000) //pattern A
							{
								PatchB(FOffset+0x64, FOffset+0x14);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else if(read32(FOffset + 0x14) == 0x2C030000 && read32(FOffset + 0x34) == 0x28000001 &&
								read32(FOffset + 0x54) == 0x3860FFFF && read32(FOffset + 0x5C) == 0x38600000) //pattern B
							{
								PatchB(FOffset+0x60, FOffset+0x14);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DHCP_request_nb:
						{
							//only detects 0x20 size cause of the blr, this checks for the cmps past that to verify
							if(read32(FOffset + 0xC) == 0x2C000000 && read32(FOffset + 0x24) == 0x28070000  &&
								read32(FOffset + 0x2C) == 0x28080000 && read32(FOffset + 0x34) == 0x28090000) //pattern A
							{
								DHCPStateOffset = read16(FOffset+0xA); //dhcp state offset
								memcpy((void*)FOffset, DHCP_request_nb, DHCP_request_nb_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else if(read32(FOffset + 0x20) == 0x2C000000 && read32(FOffset + 0x38) == 0x28070000  &&
								read32(FOffset + 0x40) == 0x28080000 && read32(FOffset + 0x48) == 0x28090000) //pattern B
							{
								DHCPStateOffset = read16(FOffset+0x1E); //dhcp state offset
								memcpy((void*)FOffset, DHCP_request_nb, DHCP_request_nb_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DHCP_hostname:
						{
							if(read32(FOffset + 0x8) == 0x38000000 && read32(FOffset + 0x38) == 0x5464043E &&
								((read32(FOffset + 0x3C) == 0x28040020) || //older lib
								(read32(FOffset + 0x3C) == 0x28040100)) //newer lib
								&& read32(FOffset + 0x44) == 0x28040000)
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DHCP_get_state:
						{
							//mirror behavior of later library versions, return state immediately
							PatchB(FOffset+0x1A4, FOffset+0x10);
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE_DHCP_get_gateway:
						{
							if(read32(FOffset + 0x18) == 0x2C000003 && read32(FOffset + 0x24) == 0x2C000004 &&
								read32(FOffset + 0x30) == 0x2C000005)
							{
								memcpy((void*)FOffset, DHCP_get_gateway, DHCP_get_gateway_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DHCP_release:
						{
							if((read32(FOffset + 0x4) == 0x38600006 && read32(FOffset + 0xC) == 0x38000000 &&
								read32(FOffset + 0x48) == 0x3860FFFF && read32(FOffset + 0x50) == 0x38600000) || //pattern A
								(read32(FOffset + 0x4) == 0x38600006 && read32(FOffset + 0xC) == 0x38000000 &&
								read32(FOffset + 0x44) == 0x3860FFFF && read32(FOffset + 0x4C) == 0x38600000) || //pattern B
								(read32(FOffset + 0x14) == 0x2C000000 && read32(FOffset + 0x24) == 0x38000006 &&
								read32(FOffset + 0x44) == 0x3860FFFF && read32(FOffset + 0x70) == 0x38600000)) //pattern C
							{
								memcpy((void*)FOffset, DHCP_release, DHCP_release_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DHCP_release_nb:
						{
							if(read32(FOffset + 0x4) == 0x2C000000 && read32(FOffset + 0x8) == 0x4080000C &&
								read32(FOffset + 0xC) == 0x3860FFFF && read32(FOffset + 0x14) == 0x38600006)
							{
								memcpy((void*)FOffset, DHCP_release, DHCP_release_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_DHCP_terminate:
						{
							if((read32(FOffset + 0x10) == 0x2C03FFFF && read32(FOffset + 0x1C) == 0x3800FFFF &&
								read32(FOffset + 0x24) == 0x38600000) || //pattern A
								(read32(FOffset + 0x1C) == 0x2C03FFFF && read32(FOffset + 0x28) == 0x3800FFFF &&
								read32(FOffset + 0x30) == 0x38600000)) //pattern B/C
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_tcp_create:
						{
							if(read32(FOffset + 0x18) == 0x807F0000 && read32(FOffset + 0x24) == 0x7C601B78 &&
								read32(FOffset + 0x28) == 0x807F0000 && read32(FOffset + 0x2C) == 0x7C1F0378)
							{
								memcpy((void*)FOffset, tcp_create, tcp_create_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_tcp_setsockopt:
						//detection IDENTICAL to tcp_setsockopt, only way to differenciate is function order,
						//tcp_setsockopt happens before tcp_getsockopt does
						case FCODE_tcp_getsockopt:
						{
							if(read32(FOffset + 0x2C) == 0x3860FFFE && read32(FOffset + 0x40) == 0x3860FFFE &&
								read32(FOffset + 0x68) == 0x7C601B78)
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_tcp_stat:
						{
							if(read32(FOffset + 0x28) == 0x3860FFFC && read32(FOffset + 0x5C) == 0x3860FFFC &&
								read32(FOffset + 0x7C) == 0x7C601B78)
							{
								memcpy((void*)FOffset, tcp_stat, tcp_stat_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						//detection IDENTICAL to tcp_stat, only way to differenciate is function order,
						//tcp_stat happens before tcp_getaddr does
						case FCODE_tcp_getaddr:
						{
							if(read32(FOffset + 0x28) == 0x3860FFFC && read32(FOffset + 0x5C) == 0x3860FFFC &&
								read32(FOffset + 0x7C) == 0x7C601B78)
							{
								memcpy((void*)FOffset, tcp_getaddr, tcp_getaddr_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_tcp_cancel:
						{
							if(read32(FOffset + 0x24) == 0x3860FFFC && read32(FOffset + 0x58) == 0x3860FFFC &&
								read32(FOffset + 0x6C) == 0x7C601B78)
							{
								memcpy((void*)FOffset, tcp_delete, tcp_delete_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_tcp_delete:
						{
							if(read32(FOffset + 0x1C) == 0x3860FFFC && read32(FOffset + 0x40) == 0x7C601B78)
							{
								memcpy((void*)FOffset, tcp_delete, tcp_delete_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_udp_close:
						{
							if(read32(FOffset + 0x8) == 0x5460052A && read32(FOffset + 0xC) == 0x2C000400 &&
								read32(FOffset + 0x20) == 0x3860FFFC)
							{
								memcpy((void*)FOffset, ReturnMinus1, ReturnMinus1_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_route4_add:
						{
							if(read32(FOffset + 0x44) == 0x38C00001 && read32(FOffset + 0x48) == 0x38E00000 &&
								read32(FOffset + 0x4C) == 0x39000000)
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_route4_del:
						{
							if(read32(FOffset + 0x44) == 0x38C00000 && read32(FOffset + 0x4C) == 0x7C601B78 &&
								((read32(FOffset + 0x5C) == 0x7FE3FB78) || //older lib
								(read32(FOffset + 0x60) == 0x7FE3FB78))) //newer lib
								
							{
								memcpy((void*)FOffset, Return0, Return0_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_GetEtherLinkState:
						{
							if(read32(FOffset + 0x10) == 0x2C03FFFF && read32(FOffset + 0x24) == 0x2C030001 &&
								read32(FOffset + 0x40) == 0x38600000 && read32(FOffset + 0x48) == 0x3860FFFF)
							{
								memcpy((void*)FOffset, Return1, Return1_size);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_GetConnectionType:
						{
							if(read32(FOffset + 0x5C) == 0x3C000202 && read32(FOffset + 0x78) == 0x3C600402 &&
								read32(FOffset + 0x7C) == 0x38030200)
							{
								//detect as BBA
								PatchB(FOffset+0xB4, FOffset+0x40);
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else //false hit
								CurPatterns[j].Found = 0;
						} break;
						default:
						{
							if( CurPatterns[j].Patch == (u8*)ARQPostRequest )
							{
								if ((  (TITLE_ID) != 0x474D53  // Super Mario Sunshine
									&& (TITLE_ID) != 0x474C4D  // Luigis Mansion
									&& (TITLE_ID) != 0x475049  // Pikmin
									&& (TITLE_ID) != 0x474C56  // Chronicles of Narnia
									&& !DemoNeedsPostRequest())
									|| useipl == 1)
								{
									dbgprintf("Patch:[ARQPostRequest] skipped (0x%08X)\r\n", FOffset);
									break;
								}
							}
							if( CurPatterns[j].Patch == DVDGetDriveStatus )
							{
								if( (TITLE_ID) != 0x474754 &&	// Chibi-Robo!
									(TITLE_ID) != 0x475041 )	// Pok駑on Channel
									break;
							}
							if( (CurPatterns[j].Length >> 16) == (FCODES  >> 16) )
							{
								dbgprintf("Patch:Unhandled dead case:%08X\r\n", CurPatterns[j].Length );
							}
							else
							{
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								memcpy( (void*)(FOffset), CurPatterns[j].Patch, CurPatterns[j].PatchLength );
							}
						} break;
					}
					// If this is a patch group set all others of this group as found aswell
					if( CurPatterns[j].Group && CurPatterns[j].Found )
					{
						for( k=0; k < CurPatternsLen; ++k )
						{
							if( CurPatterns[k].Group == CurPatterns[j].Group )
							{
								if( !CurPatterns[k].Found )		// Don't overwrite the offset!
									CurPatterns[k].Found = -1;	// Usually this holds the offset, to determinate it from a REALLY found pattern we set it -1 which still counts a logical TRUE
								//dbgprintf("Setting [%s] to found!\r\n", CurPatterns[k].Name );
							}
						}
					}
					// We dont need to compare this to more patterns
					i += (CurPatterns[j].Length - 4);
					patfound = true;
					break;
				}
			}
			if(patfound) break;
		}
	}
	// Not really Triforce Arcade. Just borrowing the config. Replaces GBA games on AGP disc.
	if (Datel && ConfigGetConfig(NIN_CFG_ARCADE_MODE))
	{
		if(write32A(0x00098044, 0x08900000, 0x09900000, 0))
			printpatchfound("Absolute Zed", "Bounty Hunter", 0x00098044);
		if(write32A(0x00098050, 0x09100000, 0x0A900000, 0))
			printpatchfound("Alien Invasion 2", "Chopper 2", 0x00098050);
		if(write32A(0x0009805C, 0x0A100000, 0x0B100000, 0))
			printpatchfound("In The Garden", "Dragon Tiles 3", 0x0009805C);
		if(write32A(0x00098068, 0x0C900000, 0x0B900000, 0))
			printpatchfound("Llamaboost", "Invaders", 0x00098068);
		if(write32A(0x00098074, 0x10100000, 0x0C100000, 0))
			printpatchfound("Puzzle", "Jetpack 2", 0x00098074);
		if(write32A(0x00098080, 0x10900000, 0x0D900000, 0))
			printpatchfound("Q Zone", "Loop The Loop", 0x00098080);
		if(write32A(0x0009808C, 0x11100000, 0x0E100000, 0))
			printpatchfound("Rally Z", "Paddle Panic", 0x0009808C);
		if(write32A(0x00098098, 0x11900000, 0x0E900000, 0))
			printpatchfound("Space Loonies", "Popem", 0x00098098);
		if(write32A(0x000980A4, 0x12100000, 0x0F900000, 0))
			printpatchfound("Stick a Brick", "Proxima", 0x000980A4);
	}

	/* Check for PADInit, if not found use SIInit */
	if(SIInitStoreAddr != 0)
	{
		if(PADInitOffset != 0)
		{
			PatchB(SIInitStoreAddr, PADInitOffset);
			printpatchfound("PADInit", NULL, PADInitOffset);
		}
		else if(SIInitOffset != 0)
		{
			PatchB(SIInitStoreAddr, SIInitOffset);
			printpatchfound("SIInit", NULL, SIInitOffset);
		}
	}
	if(PatchWide)
	{
		if(MTXPerspectiveOffset != 0)
		{
			printpatchfound("C_MTXPerspective", NULL, MTXPerspectiveOffset);
			PatchWideMulti(MTXPerspectiveOffset + 0x20, 29);
		}
		if(MTXLightPerspectiveOffset != 0)
		{
			printpatchfound("C_MTXLightPerspective", NULL, MTXLightPerspectiveOffset);
			PatchWideMulti(MTXLightPerspectiveOffset + 0x24, 27);
		}
	}
	if(cheatsWanted || debuggerWanted)
	{
		//setup jump to codehandler stub
		if(OSSleepThreadHook || PADHook)
		{
			u32 codehandler_stub_offset = PatchCopy(codehandler_stub, codehandler_stub_size);
			if(OSSleepThreadHook) PatchB( codehandler_stub_offset, OSSleepThreadHook );
			if(PADHook) PatchB( codehandler_stub_offset, PADHook );
		}
		u32 cheats_start;
		if(debuggerWanted)
		{
			//copy into dedicated space
			memcpy( (void*)0x1000, codehandler, codehandler_size );
			//copy game id for debugger
			memcpy((void*)0x1800, (void*)0, 8);
			//main code area start
			cheats_start = 0x1000 + codehandler_size - 8;
		}
		else
		{
			//copy into dedicated space
			memcpy( (void*)0x1000, codehandleronly, codehandleronly_size );
			//main code area start
			cheats_start = 0x1000 + codehandleronly_size - 8;
		}
		u32 cheats_area = (POffset < cheats_start) ? 0 : (POffset - cheats_start);
		if(cheats_area > 0)
		{
			dbgprintf("Possible Code Size: %08x\r\n", cheats_area);
			memset((void*)cheats_start, 0, cheats_area);
		}
		//copy in gct file if requested
		if( cheatsWanted && TRIGame != TRI_SB && useipl == 0 )
		{
			FIL CodeFD;
			if( Check_Cheats() == 0 && f_open_char( &CodeFD, cheatPath, FA_OPEN_EXISTING|FA_READ ) == FR_OK )
			{
				if( CodeFD.obj.objsize > cheats_area )
				{
					dbgprintf("Patch:Cheatfile is too large, it must not be larger than %i bytes!\r\n", cheats_area);
				}
				else
				{
					void *CMem = malloc(CodeFD.obj.objsize);
					if( f_read( &CodeFD, CMem, CodeFD.obj.objsize, &read ) == FR_OK )
					{
						memcpy((void*)cheats_start, CMem, CodeFD.obj.objsize);
						sync_after_write((void*)cheats_start, CodeFD.obj.objsize);
						dbgprintf("Patch:Copied %s to memory\r\n", cheatPath);
					}
					else
					{
						dbgprintf("Patch:Failed to read %s\r\n", cheatPath);
					}
					free( CMem );
				}
				f_close( &CodeFD );
			}
			else
			{
				dbgprintf("Patch:Failed to open/find cheat file:\"%s\"\r\n", cheatPath );
			}
		}

		// Debug Wait setting.
		vu32 *debug_wait = (vu32*)(P2C(*(vu32*)0x1000));
		if( IsWiiU() )
		{
			// Debugger is not supported on Wii U.
			*debug_wait = 0;
		}
		else
		{
			if (ConfigGetConfig(NIN_CFG_DEBUGWAIT)) {
				*debug_wait = 1;
			} else {
				*debug_wait = 0;
			}
		}
		//if(DebuggerHook) PatchB( codehandler_stub_offset, DebuggerHook );
		//if(DebuggerHook2) PatchB( codehandler_stub_offset, DebuggerHook2 );
		//if(DebuggerHook3) PatchB( codehandler_stub_offset, DebuggerHook3 );
	}
	free(hash);
	free(SHA1i);

	/*if( ((PatchCount & (1|2|4|8|2048)) != (1|2|4|8|2048)) )
	{
		dbgprintf("Patch:Could not apply all required patches!\r\n");
		Shutdown();
	}*/

	#ifdef DEBUG_PATCH
	//dbgprintf("PatchCount:%08X\r\n", PatchCount );
	//if( (PatchCount & FPATCH_DSP_ROM) == 0 )
	//	dbgprintf("Patch:Unknown DSP ROM\r\n");

	for(patitr = 0; patitr < CurFPatternsListLen; ++patitr)
	{
		FuncPattern *CurPatterns = CurFPatternsList[patitr].pat;
		u32 CurPatternsLen = CurFPatternsList[patitr].patlen;
		for( j=0; j < CurPatternsLen; ++j )
		{
			if(!CurPatterns[j].Found)
			{
				dbgprintf("Patch:[%s] not found\r\n", CurPatterns[j].Name );
				if(CurPatterns[j].Group != FGROUP_NONE)
					for( k = j; (j+1) < CurPatternsLen && CurPatterns[j+1].Group == CurPatterns[k].Group; ++j ) ;
			}
		}
	}
	#endif
	PatchState = PATCH_STATE_DONE;

	if(bbaEmuWanted == 2)
	{
		if(OSSleepThread && OSWakeupThread && OSCreateThread && OSResumeThread && DHCPStateOffset)
		{
			u32 hspAddr = PatchCopy(HSPIntrruptHandler, HSPIntrruptHandler_size);
			PatchBL(OSWakeupThread, hspAddr+0x28);
			sync_after_write((void*)hspAddr, HSPIntrruptHandler_size);
			dbgprintf("Patch:Inserted HSP Interrupt Handler\r\n");
			write32(SO_HSP_LOC+0x00, (1<<31)|hspAddr);
			write32(SO_HSP_LOC+0x04, (1<<31)|OSSleepThread);
			write32(SO_HSP_LOC+0x08, (1<<31)|OSWakeupThread);
			write32(SO_HSP_LOC+0x0C, (1<<31)|OSCreateThread);
			write32(SO_HSP_LOC+0x10, (1<<31)|OSResumeThread);
			write32(SO_HSP_LOC+0x14, DHCPStateOffset<<16 | 0x7777); //skips start set in IOSInterface
			u16 SOShift;
			if(isPSO)
			{
				SOShift = 15; //shift 15, 32k
				SOCurrentTotalFDs = 16;
			}
			else
			{
				SOShift = 13; //shift 13, 8k
				SOCurrentTotalFDs = 64;
			}
			write32(SO_HSP_LOC+0x18, SOShift << 16 | SOCurrentTotalFDs);
			write32(SO_HSP_LOC+0x1C, 0); //needs to be inited
			sync_after_write((void*)SO_HSP_LOC, 0x20);
			if(TITLE_ID == 0x474845)
			{
				if(read32(0x6E88) == 0x4804E39D)
				{
					memcpy((void*)0x6E70, Return1, Return1_size);
					dbgprintf("Patch:Homeland Main BBA Detection 1\r\n");
				}
				if(read32(0x54B90) == 0x480002E5)
				{
					write32(0x54B84, 0x3C600402);
					write32(0x54B88, 0x60630200);
					write32(0x54B90, 0x90650000);
					dbgprintf("Patch:Homeland Main BBA Detection 2\r\n");
				}
			}
		}
		else if(TITLE_ID == 0x474845 && read32(0x5DD4) == 0x480AF13D)
		{
			write32(0x5DCC, 0x3C600402);
			write32(0x5DD0, 0x60630200);
			write32(0x5DD4, 0x90650000);
			dbgprintf("Patch:Homeland NetCFG BBA Detection\r\n");
		}
	}
	else if(bbaEmuWanted == 1 && OSSleepThread && OSWakeupThread && SOStartedOffset)
	{
		u32 hspAddr = PatchCopy(HSPIntrruptHandler, HSPIntrruptHandler_size);
		PatchBL(OSWakeupThread, hspAddr+0x28);
		sync_after_write((void*)hspAddr, HSPIntrruptHandler_size);
		dbgprintf("Patch:Inserted HSP Interrupt Handler\r\n");
		write32(SO_HSP_LOC+0x00, (1<<31)|hspAddr);
		write32(SO_HSP_LOC+0x04, (1<<31)|OSSleepThread);
		write32(SO_HSP_LOC+0x14, SOStartedOffset);
		u16 SOShift = 14; //shift 14, 16k
		SOCurrentTotalFDs = 32;
		write32(SO_HSP_LOC+0x18, SOShift << 16 | SOCurrentTotalFDs);
		write32(SO_HSP_LOC+0x1C, 0); //needs to be inited
		sync_after_write((void*)SO_HSP_LOC, 0x20);
		if(TITLE_ID == 0x474D34)
		{
			if(read32(0x001DD168) == 0x4BEE812D)
			{
				PatchB(0x001DD184, 0x001DD168);
				dbgprintf("Patch:Patched Mario Kart NTSC-J BBA Detection\r\n");
			}
			else if(read32(0x001DD140) == 0x4BEE8155)
			{
				PatchB(0x001DD15C, 0x001DD140);
				dbgprintf("Patch:Patched Mario Kart NTSC-U BBA Detection\r\n");
			}
			else if(read32(0x001DD100) == 0x4BEE8159)
			{
				PatchB(0x001DD11C, 0x001DD100);
				dbgprintf("Patch:Patched Mario Kart PAL BBA Detection\r\n");
			}
		}
		else if(TITLE_ID == 0x474B59)
		{
			if(read32(0x00482124) == 0x4BF6473D && read32(0x004821A8) == 0x4BF646B9)
			{
				PatchB(0x00482148, 0x00482124);
				PatchB(0x004821D8, 0x004821A8);
				dbgprintf("Patch:Patched Kirby Air Ride NTSC-J BBA Detection\r\n");
			}
			else if(read32(0x00487204) == 0x4BF6473D && read32(0x00487288) == 0x4BF646B9)
			{
				PatchB(0x00487228, 0x00487204);
				PatchB(0x004872B8, 0x00487288);
				dbgprintf("Patch:Patched Kirby Air Ride NTSC-U BBA Detection\r\n");
			}
			else if(read32(0x0048AF30) == 0x4BF63051 && read32(0x0048AFB4) == 0x4BF62FCD)
			{
				PatchB(0x0048AF54, 0x0048AF30);
				PatchB(0x0048AFE4, 0x0048AFB4);
				dbgprintf("Patch:Patched Kirby Air Ride PAL BBA Detection\r\n");
			}
		}
		else if(TITLE_ID == 0x475445)
		{
			if(read32(0x00098C84) == 0x480A821D && read32(0x000F22A8) == 0x4804EBF9)
			{
				PatchB(0x00098C98, 0x00098C84);
				PatchB(0x000F22BC, 0x000F22A8);
				dbgprintf("Patch:1080 Avalanche NTSC-J BBA Detection\r\n");
			}
			else if(read32(0x00097C50) == 0x480A549D && read32(0x000EEFC8) == 0x4804E125)
			{
				PatchB(0x00097C64, 0x00097C50);
				PatchB(0x000EEFDC, 0x000EEFC8);
				dbgprintf("Patch:1080 Avalanche NTSC-U BBA Detection\r\n");
			}
			else if(read32(0x00097EB8) == 0x480A5EFD && read32(0x000EF920) == 0x4804E495)
			{
				PatchB(0x00097ECC, 0x00097EB8);
				PatchB(0x000EF934, 0x000EF920);
				dbgprintf("Patch:1080 Avalanche PAL BBA Detection\r\n");
			}
		}
	}
	//Sonic R NTSC Old Debug Prints
	/*if(read32(0x7D49C) == 0x9421FF90 && read32(0x7D5A8) == 0x9421FF90)
	{
		memcpy((void*)0x7D49C, OSReportDM, sizeof(OSReportDM));
		sync_after_write((void*)0x7D49C, sizeof(OSReportDM));
		memcpy((void*)0x7D5A8, OSReportDM, sizeof(OSReportDM));
		sync_after_write((void*)0x7D5A8, sizeof(OSReportDM));
		
	}*/
	/*if(GAME_ID == 0x47365145) //Megaman Collection
	{
		memcpy((void*)0x5A110, OSReportDM, sizeof(OSReportDM));
		sync_after_write((void*)0x5A110, sizeof(OSReportDM));

		memcpy((void*)0x820FC, OSReportDM, sizeof(OSReportDM));
		sync_after_write((void*)0x820FC, sizeof(OSReportDM));

		#ifdef DEBUG_PATCH
		dbgprintf("Patch:Patched Megaman Collection\r\n");
		#endif
	}
	if(GAME_ID == 0x47585845) // Pokemon XD
	{
		memcpy((void*)0x2A65CC, OSReportDM, sizeof(OSReportDM));
		sync_after_write((void*)0x2A65CC, sizeof(OSReportDM));
	}
	if(GAME_ID == 0x47435345) // Street Racing Syndicate
	{
		//UnkReport
		memcpy((void*)0x16884, OSReportDM, sizeof(OSReportDM));
		sync_after_write((void*)0x16884, sizeof(OSReportDM));
		//debug switch
		write32(0x330E68, 0);
		//OSReport
		memcpy((void*)0x235A4, OSReportDM, sizeof(OSReportDM));
		sync_after_write((void*)0x235A4, sizeof(OSReportDM));
		//failed expermiments, someday I'll figure it out...
		//write32(0xF4B4,0x38600000);
		write32(0x1F6434,0x38600000);
		write32(0x1F6454,0x60000000);
		write32(0x1F6464,0x60000000);
		write32(0x1F646C,0x38800000);
		write32(0x2583E8,0x38600000);
		write32(0x2583EC,0x4E800020);
		dbgprintf("Patch:Patched Street Racing Syndicate NTSC-U\r\n");
	}*/
	if(useipl == 1)
	{
		//Force no audio resampling to avoid crashing
		if(read32(0x1341514) == 0x38600001)
		{
			write32(0x1341514, 0x38600000);
			//fix up dsp init for cardunlock
			write32(0x134F28C, 0x64001000); //oris r0, r0, 0x1000
			dbgprintf("Patch:Patched Gamecube NTSC IPL v1.0\r\n");
		}
		else if(read32(0x13693D4) == 0x38600001)
		{
			write32(0x13693D4, 0x38600000);
			dbgprintf("Patch:Patched Gamecube NTSC IPL v1.1\r\n");
		}
		else if(read32(0x13702A0) == 0x38600001)
		{
			write32(0x13702A0, 0x38600000);
			dbgprintf("Patch:Patched Gamecube NTSC IPL v1.2\r\n");
		}
		else if(read32(0x136C9B4) == 0x38600001)
		{
			write32(0x136C9B4, 0x38600000);
			dbgprintf("Patch:Patched Gamecube PAL IPL v1.0\r\n");
		}
		else if(read32(0x1373618) == 0x38600001)
		{
			write32(0x1373618, 0x38600000);
			dbgprintf("Patch:Patched Gamecube PAL IPL v1.2\r\n");
		}
		else if(read32(0x13692F4) == 0x38600001)
		{
			write32(0x13692F4, 0x38600000);
			dbgprintf("Patch:Patched Gamecube MPAL IPL v1.1\r\n");
		}
	}
	else if( TITLE_ID == 0x474256 )	// Batman Vengeance
	{
		// Skip Usage of EXI Channel 2
		if(write32A(0x0018B5DC, 0x60000000, 0x4801D6E1, 0))
		{
			dbgprintf("Patch:Patched Batman Vengeance NTSC-U\r\n");
		}
		else if(write32A(0x0015092C, 0x60000000, 0x4801D599, 0))
		{
			dbgprintf("Patch:Patched Batman Vengeance PAL\r\n");
		}
	}
	else if( TITLE_ID == 0x474336 )	// Pokemon Colosseum
	{
		// Memory Card inserted hack
		if( DisableEXIPatch == 0 )
		{
			if(write32A(0x000B0D88, 0x38600001, 0x4801C0B1, 0))
			{
				dbgprintf("Patch:Patched Pokemon Colosseum NTSC-J MemCard Emu\r\n");
			}
			else if(write32A(0x000B30DC, 0x38600001, 0x4801C2ED, 0))
			{
				dbgprintf("Patch:Patched Pokemon Colosseum NTSC-U MemCard Emu\r\n");
			}
			else if(write32A(0x000B66DC, 0x38600001, 0x4801C2ED, 0))
			{
				dbgprintf("Patch:Patched Pokemon Colosseum PAL MemCard Emu\r\n");
			}
		}
		// Controller Invert Bug
		if(read32(0x000F6030) == 0x7C0000D0 && read32(0x000F603C) == 0x7C0000D0)
		{
			write32(0x000F6030, 0x7C0000F8); //neg->not
			write32(0x000F603C, 0x7C0000F8); //neg->not
			dbgprintf("Patch:Patched Pokemon Colosseum NTSC-J Controller Bug\r\n");
		}
		else if(read32(0x000F8368) == 0x7C0000D0 && read32(0x000F8374) == 0x7C0000D0)
		{
			write32(0x000F8368, 0x7C0000F8); //neg->not
			write32(0x000F8374, 0x7C0000F8); //neg->not
			dbgprintf("Patch:Patched Pokemon Colosseum NTSC-U Controller Bug\r\n");
		}
		else if(read32(0x000FB9F8) == 0x7C0000D0 && read32(0x000FBA04) == 0x7C0000D0)
		{
			write32(0x000FB9F8, 0x7C0000F8); //neg->not
			write32(0x000FBA04, 0x7C0000F8); //neg->not
			dbgprintf("Patch:Patched Pokemon Colosseum PAL Controller Bug\r\n");
		}
	}
	else if( GAME_ID == 0x5043534A ) // Colosseum Bonus
	{
		// Memory Card inserted hack
		if( DisableEXIPatch == 0 && write32A(0x000B0474, 0x38600001, 0x4801C0B5, 0) )
			dbgprintf("Patch:Patched Pokemon Colosseum Bonus NTSC-J MemCard Emu\r\n");
		// Controller Invert Bug
		if(read32(0x000F5784) == 0x7C0000D0 && read32(0x000F5790) == 0x7C0000D0)
		{
			write32(0x000F5784, 0x7C0000F8); //neg->not
			write32(0x000F5790, 0x7C0000F8); //neg->not
			dbgprintf("Patch:Patched Pokemon Colosseum Bonus NTSC-J Controller Bug\r\n");
		}
	}
	else if( TITLE_ID == 0x475858 )	// Pokemon XD
	{
		// Controller Invert Bug
		if(read32(0x00100DA0) == 0x7CC000D0 && read32(0x00100DAC) == 0x7CC000D0)
		{
			write32(0x00100DA0, 0x7C0600F8); //neg->not
			write32(0x00100DAC, 0x7C0600F8); //neg->not
			dbgprintf("Patch:Patched Pokemon XD NTSC-J Controller Bug\r\n");
		}
		else if(read32(0x00104A1C) == 0x7CC000D0 && read32(0x00104A28) == 0x7CC000D0)
		{
			write32(0x00104A1C, 0x7C0600F8); //neg->not
			write32(0x00104A28, 0x7C0600F8); //neg->not
			dbgprintf("Patch:Patched Pokemon XD NTSC-U Controller Bug\r\n");
		}
		else if(read32(0x0010607C) == 0x7CC000D0 && read32(0x00106088) == 0x7CC000D0)
		{
			write32(0x0010607C, 0x7C0600F8); //neg->not
			write32(0x00106088, 0x7C0600F8); //neg->not
			dbgprintf("Patch:Patched Pokemon XD PAL Controller Bug\r\n");
		}
	}
	else if( TITLE_ID == 0x475A4C )	// GZL=Wind Waker
	{
		//Anti FrameDrop Panic
		/* NTSC-U Final */
		if(read32(0x00221A28) == 0x40820034 && read32(0x00256424) == 0x41820068)
		{
			//	write32( 0x03945B0, 0x8039408C );	// Test menu
			write32(0x00221A28, 0x48000034);
			write32(0x00256424, 0x48000068);
			dbgprintf("Patch:Patched WW NTSC-U\r\n");
		}
		/* NTSC-U Demo */
		if(read32(0x0021D33C) == 0x40820034 && read32(0x00251EF8) == 0x41820068)
		{
			write32(0x0021D33C, 0x48000034);
			write32(0x00251EF8, 0x48000068);
			dbgprintf("Patch:Patched WW NTSC-U Demo\r\n");
		}
		/* PAL Final */
		if(read32(0x001F1FE0) == 0x40820034 && read32(0x0025B5C4) == 0x41820068)
		{
			write32(0x001F1FE0, 0x48000034);
			write32(0x0025B5C4, 0x48000068);
			dbgprintf("Patch:Patched WW PAL\r\n");
		}
		/* NTSC-J Final */
		if(read32(0x0021EDD4) == 0x40820034 && read32(0x00253BCC) == 0x41820068)
		{
			write32(0x0021EDD4, 0x48000034);
			write32(0x00253BCC, 0x48000068);
			dbgprintf("Patch:Patched WW NTSC-J\r\n");
		}
		/*if( ConfigGetConfig( NIN_CFG_OSREPORT ) )
		{
			*(vu32*)(0x00006858) = 0x60000000;		//OSReport disabled patch
			PatchB( 0x00006950, 0x003191BC );
		}*/
	}
	else if( TITLE_ID == 0x475832 )	// X-Men Legends 2
	{
		//Fix a Bad Jump in the code
		if(write32A(0x0018A764, 0x4182021C, 0x41820018, 0))
		{
			dbgprintf("Patch:Patched X-Men Legends 2 NTSC-U\r\n");
		}
	}
	else if( TITLE_ID == 0x474159 ) // Midway Arcarde Treasures 2
	{
		//Skip over __AXOutInit calling AIStartDMA
		if(MAT2patched == 0 && write32A(0x906D0, 0x60000000, 0x4BFFB6D9, 0))
		{
			//needs patch on first bootup, no idea why
			MAT2patched = 1;
			//UnkReport
			//PatchB(0x7C964,0x92F50);
			//call AIStartDMA after NGC_SoundInit
			PatchB(0x8BDA8,0x4F1B8);
			dbgprintf("Patch:Patched Midway Arcade Treasures 2 NTSC-U\r\n");
		}
	}
	else if( TITLE_ID == 0x475051 ) // Powerpuff Girls
	{
		// Audio Stream force DMA to get Video Sound
		if(read32(0xF33D8) == 0x4E800020 && read32(0xF3494) == 0x4E800020)
		{
			//UnkReport
			//memcpy((void*)0xDF870, OSReportDM, sizeof(OSReportDM));
			//sync_after_write((void*)0xDF870, sizeof(OSReportDM));
			//OSReport
			//memcpy((void*)0xE84D4, OSReportDM, sizeof(OSReportDM));
			//sync_after_write((void*)0xE84D4, sizeof(OSReportDM));
			//Call AXInit after DVDPrepareStreamAbsAsync
			PatchB(0xF9E80, 0xF33D8);
			//Call AXQuit after DVDCancelStreamAsync
			PatchB(0xF9EB4, 0xF3494);
			dbgprintf("Patch:Patched Powerpuff Girls NTSC-U\r\n");
		}
		else if(read32(0xF3EC0) == 0x4E800020 && read32(0xF3F7C) == 0x4E800020)
		{
			//Call AXInit after DVDPrepareStreamAbsAsync
			PatchB(0xFB340, 0xF3EC0);
			//Call AXQuit after DVDCancelStreamAsync
			PatchB(0xFB3B0, 0xF3F7C);
			dbgprintf("Patch:Patched Powerpuff Girls PAL\r\n");
		}
	}
	else if( TITLE_ID == 0x47505A ) // Nintendo Puzzle Collection
	{
		if(read32(0x6A28) == 0x7C7F282E && read32(0x6AF8) == 0x7C1F002E)
		{
			//Dont load compressed game .rel files but the uncompressed 
			//ones to patch the timers without any further hooks
			write32(0x6A28, 0x38600000);
			write32(0x6AF8, 0x38000000);
			dbgprintf("Patch:Patched Nintendo Puzzle Collection NTSC-J\r\n");
		}
	}
	else if( TITLE_ID == 0x44504F ) //PSO Demo JAP
	{
		//skip modem detection error to let demo boot up
		if(write32A(0x194F40, 0x4182002C, 0x4082002C, 0))
		{
			dbgprintf("Patch:Patched Phantasy Star Online Demo NTSC-J\r\n");
		}
	}
	else if( GAME_ID == 0x33303145 )
	{
		if(read32(0x110F898) == 0x3C630C00 && read32(0x110F8A0) == 0x6403C000)
		{
			//UnkReport
			//memcpy((void*)0x110F100, OSReportDM, sizeof(OSReportDM));
			//sync_after_write((void*)0x110F100, sizeof(OSReportDM));
			//UnkReport2
			//write32(0x110F1C8, 0x7C832378); // mr  r3,r4
			//write32(0x110F1CC, 0x7CA42B78); // mr  r4,r5
			//memcpy((void*)0x110F1D0, OSReportDM, sizeof(OSReportDM));
			//sync_after_write((void*)0x110F1D0, sizeof(OSReportDM));
			//DI Regs get set up weirdly so PatchFuncInterface misses this
			write32(0x110F898, 0x60000000); // nop this line out, not needed
			write32(0x110F8A0, 0x6403D302); // Patch to: oris r3, r0, 0xD302
			dbgprintf("Patch:Patched GameCube Service Disc NTSC-U\r\n");
		}
	}
	if(videoPatches)
	{
		bool video60hzPatch = false;
		if( ConfigGetConfig(NIN_CFG_FORCE_PROG) )
			video60hzPatch = true; //480p 60hz
		else if( ConfigGetVideoMode() & NIN_VID_FORCE )
		{
			u8 NinForceMode = ConfigGetVideoMode() & NIN_VID_FORCE_MASK;
			if(NinForceMode == NIN_VID_FORCE_PAL60 || NinForceMode == NIN_VID_FORCE_NTSC 
					|| NinForceMode == NIN_VID_FORCE_MPAL)
				video60hzPatch = true; //480i 60hz
		}
		if( TITLE_ID == 0x47454F ) // Capcom vs. SNK 2 EO
		{
			//fix for force progressive
			if(write32A(0x11224, 0x60000000, 0xB0010010, 0))
			{
				dbgprintf("Patch:Patched Capcom vs. SNK 2 EO NTSC-J\r\n");
			}
			else if(write32A(0x1137C, 0x60000000, 0xB0010010, 0))
			{
				dbgprintf("Patch:Patched Capcom vs. SNK 2 EO NTSC-U\r\n");
			}
		}
		else if( TITLE_ID == 0x475038 ) // Pac-Man World 3
		{
			//fix for force progressive
			if(write32A(0x28D2A8, 0x48000010, 0x41820010, 0))
			{
				dbgprintf("Patch:Patched Pac-Man World 3 NTSC-U\r\n");
			}
		}
		else if( TITLE_ID == 0x475134 ) // SpongeBob SquarePants CFTKK
		{
			//fix for force progressive
			if(write32A(0x23007C, 0x48000010, 0x41820010, 0))
			{
				dbgprintf("Patch:Patched SpongeBob SquarePants CFTKK NTSC-U\r\n");
			}
		}
		else if( TITLE_ID == 0x47414C ) // Super Smash Bros Melee
		{
			//fix for video mode breaking
			if(write32A(0x365DB0, 0x38A00280, 0xA0A7000E, 0))
			{
				dbgprintf("Patch:Patched Super Smash Bros Melee v1.00\r\n");
			}
			else if(write32A(0x366F84, 0x38A00280, 0xA0A7000E, 0))
			{
				dbgprintf("Patch:Patched Super Smash Bros Melee v1.01\r\n");
			}
			else if(write32A(0x367C64, 0x38A00280, 0xA0A7000E, 0))
			{
				dbgprintf("Patch:Patched Super Smash Bros Melee v1.02\r\n");
			}
		}
		else if( TITLE_ID == 0x475747 ) // Swingerz Golf
		{
			//while getTiming supports all video modes
			//the NTSC-U game code itself does not, so patch it
			if(read32(0x1E148) == 0x3C60801F)
			{
				PatchB(0x1E128, 0x1E148);
				dbgprintf("Patch:Patched Swingerz Golf NTSC-U\r\n");
			}
		}
		else if( GAME_ID == 0x474C4D50 ) // Luigis Mansion PAL
		{
			if(video60hzPatch && read32(0x7A44) == 0x880D00CC)
			{
				//start in PAL60
				write32(0x7A44, 0x38000001);
				dbgprintf("Patch:Patched Luigis Mansion PAL\r\n");
			}
		}
		else if( GAME_ID == 0x474B4450 ) // Doshin the Giant PAL
		{
			if(video60hzPatch && read32(0x15D04) == 0x38C00010 && read32(0x97F8C) == 0x7FE3FB78)
			{
				//start in PAL60 (cheat from Ralf@gc-forever)
				write32(0x15D04, 0x38C00000);
				write32(0x97F8C, 0x38600005);
				dbgprintf("Patch:Patched Doshin the Giant PAL\r\n");
			}
		}
		else if( GAME_ID == 0x474D5050 ) // Mario Party 4 PAL
		{
			if(video60hzPatch && read32(0x57F0) == 0x3863AE34)
			{
				//start in PAL60 (cheat from Ralf@gc-forever)
				write32(0x57F0, 0x3863AE70);
				dbgprintf("Patch:Patched Mario Party 4 PAL\r\n");
			}
		}
		else if( GAME_ID == 0x47503550 ) // Mario Party 5 PAL
		{
			if(video60hzPatch && read32(0x58E4) == 0x3863FEB4)
			{
				//start in PAL60 (cheat from Ralf@gc-forever)
				write32(0x58E4, 0x3863FEF0);
				dbgprintf("Patch:Patched Mario Party 5 PAL\r\n");
			}
		}
		else if( GAME_ID == 0x47503650 ) // Mario Party 6 PAL
		{
			if(video60hzPatch && read32(0x5964) == 0x3863C4C4)
			{
				//start in PAL60 (cheat from Ralf@gc-forever)
				write32(0x5964, 0x3863C500);
				dbgprintf("Patch:Patched Mario Party 6 PAL\r\n");
			}
		}
		else if( GAME_ID == 0x47503750 ) // Mario Party 7 PAL
		{
			if(video60hzPatch && read32(0x5964) == 0x3863BFC4)
			{
				//start in PAL60 (cheat from Ralf@gc-forever)
				write32(0x5964, 0x3863C000);
				dbgprintf("Patch:Patched Mario Party 7 PAL\r\n");
			}
		}
	}
	PatchStaticTimers();

	sync_after_write( Buffer, Length );

	UseReadLimit = 1;
	// Datel discs require read speed. (Related to Stop motor/Read Header?) 
	// Triforce titles, and the N64 Emu work better without read speed
	if((RealDiscCMD != 0 && !Datel) || TRIGame != TRI_NONE || IsN64Emu 
			|| ConfigGetConfig(NIN_CFG_REMLIMIT))
		UseReadLimit = 0;
}

void PatchInit()
{
	memcpy((void*)PATCH_OFFSET_ENTRY, FakeEntryLoad, FakeEntryLoad_size);
	sync_after_write((void*)PATCH_OFFSET_ENTRY, FakeEntryLoad_size);
	write32(PRS_DOL, 0);
	sync_after_write((void*)PRS_DOL, 0x4);
}

void SetIPL()
{
	useipl = 1;
}

void SetIPL_TRI()
{
	dbgprintf("Setup SegaBoot\r\n");
	useipltri = 1;
}

#define FLUSH_LEN (RESET_STATUS+4)
#define FLUSH_ADDR (RESET_STATUS+8)
void PatchGame()
{
	if (Datel && (AppLoaderSize != 0))
	{
		DOLMinOff = 0x01300000;
		DOLMaxOff = DOLMinOff + AppLoaderSize;
		GameEntry = 0x81300000;
		AppLoaderSize = 0;
	}
	else if(useipl)
	{
		DOLMinOff = 0x01300000;
		DOLMaxOff = 0x01600000;
		GameEntry = 0x81300000;
		sync_before_read((void*)0x1300000, 0x300000);
	}
	else if(FirstLine)
	{
		sync_before_read((void*)DOLMinOff, 0x20);
		write32(DOLMinOff, FirstLine);
		FirstLine = 0;
		sync_after_write((void*)DOLMinOff, 0x20);
		sync_before_read((void*)DOLMinOff, DOLMaxOff - DOLMinOff);
	}
	PatchState = PATCH_STATE_PATCH;
	u32 FullLength = (DOLMaxOff - DOLMinOff + 31) & (~31);
	DoPatches( (void*)DOLMinOff, FullLength, 0 );
	// Some games need special timings
	EXISetTimings(TITLE_ID, GAME_ID & 0xFF);
        if(TRIGame != TRI_SB)
        {
	      if((TITLE_ID) != 0x474645)
               {
                 ISOSetupCache();
                }
        }
	// Reset SI status
	SIInit();
	u32 SiInitSet = 0;
	// Reset Triforce
	TRIReset();
	// Didn't look for why PMW2 requires this.  ToDo
	if ((TITLE_ID) == 0x475032 || TRIGame) // PacMan World 2 and Triforce hack
		SiInitSet = 1;
	write32(0x13003060, SiInitSet); //Clear SI Inited == 0
	write32(0x13003064, PADSwitchRequired() && (useipl == 0));
	write32(0x13003068, PADForceConnected() && (useipl == 0));
	write32(0x1300306C, drcAddress); //Set on kernel boot
	write32(0x13003070, drcAddressAligned); //Set on kernel boot
	sync_after_write((void*)0x13003060, 0x20);
	/* Clear very actively used areas */
	memset32((void*)0x13026500, 0, 0x100);
	sync_after_write((void*)0x13026500, 0x100);
	/* Clear AR positions */
	memset32((void*)0x131C0040, 0, 0x20);
	sync_after_write((void*)0x131C0040, 0x20);
	/* Secure Low Mem */
	Patch31A0();
	write32( FLUSH_LEN, FullLength >> 5 );
	u32 Command2 = DOLMinOff;
	// ToDo.  HACK: Why doesn't Nightfire Deep Descent level like this??
	if ((TITLE_ID) != 0x474f37)  // 007 Nightfire
		Command2 |= 0x80000000;
	write32( FLUSH_ADDR, Command2 );
	dbgprintf("Jumping to 0x%08X\n", GameEntry);
	sync_after_write((void*)0x1000, 0x2000); //low patches
	write32( RESET_STATUS, GameEntry );
	sync_after_write((void*)RESET_STATUS, 0x20);
	// single rel patch required for some games
	NeedRelPatches = GameRelTimerPatches();
	// constant patching required in even fewer cases
	NeedConstantRelPatches = GameRelConstantTimerPatches();
	// in case we patched ipl remove status
	useipl = 0;
}

s32 Check_Cheats()
{
#ifdef CHEATS
	if( ConfigGetConfig(NIN_CFG_CHEAT_PATH) )
	{
		const char *cpath = ConfigGetCheatPath();
		if (cpath[0] != 0)
		{
			if( fileExist(cpath) )
			{
				memcpy(cheatPath, cpath, 255);
				return 0;
			}
		}
	}

	u32 i;
	const char* DiscName = ConfigGetGamePath();
	//search the string backwards for '/'
	for (i = strlen(DiscName); i > 0; --i)
	{
		if( DiscName[i] == '/' )
			break;
	}
	i++;
	memcpy(cheatPath, DiscName, i);

	//new version paths
	_sprintf(cheatPath+i, "game.gct");
	if( fileExist(cheatPath) )
		return 0;
	sync_before_read((void*)0x0, 0x20);
	_sprintf(cheatPath+i, "%.6s.gct", (char*)0x0);
	if( fileExist(cheatPath) )
		return 0;
	//gecko path
	_sprintf(cheatPath, "/codes/%.6s.gct", (char*)0x0);
	if( fileExist(cheatPath) )
		return 0;
	//oldschool backup path
	_sprintf(cheatPath, "/games/%.6s/%.6s.gct", (char*)0x0, (char*)0x0);
	if( fileExist(cheatPath) )
		return 0;
#endif
	return -1;
}
