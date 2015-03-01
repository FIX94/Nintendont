/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar
Copyright (C) 2014  FIX94

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
#include "ff.h"
#include "dol.h"
#include "elf.h"
#include "PatchCodes.h"
#include "PatchWidescreen.h"
#include "PatchTimers.h"
#include "Config.h"
#include "global.h"
#include "patches.c"
#include "DI.h"
#include "ISO.h"
#include "SI.h"
#include "EXI.h"
#include "GCAM.h"
//#define DEBUG_DSP  // Very slow!! Replace with raw dumps?

#define GAME_ID		(read32(0))
#define TITLE_ID	(GAME_ID >> 8)

#define PATCH_OFFSET_START (0x2F60 - (sizeof(u32) * 5))
#define PATCH_OFFSET_ENTRY PATCH_OFFSET_START - FakeEntryLoad_size
u32 POffset = PATCH_OFFSET_ENTRY;
vu32 Region = 0;
vu32 useipl = 0, useipltri = 0;
vu32 DisableSIPatch = 0, DisableEXIPatch = 0;
vu32 TRIGame = TRI_NONE;
extern u32 SystemRegion;
#define PRS_DOL     0x131C0000
#define PRS_EXTRACT 0x131C0020

#define PATCH_STATE_NONE  0
#define PATCH_STATE_LOAD  1
#define PATCH_STATE_PATCH 2
#define PATCH_STATE_DONE  4

#define PSO_STATE_NONE    0
#define PSO_STATE_LOAD    1
#define PSO_STATE_PSR     2
#define PSO_STATE_NOENTRY 4
#define PSO_STATE_SWITCH  8

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

extern u32 prs_decompress(void* source,void* dest);
extern u32 prs_decompress_size(void* source);

extern int dbgprintf( const char *fmt, ...);

const unsigned char DSPHashes[][0x14] =
{
	{
		0xC9, 0x7D, 0x1E, 0xD0, 0x71, 0x90, 0x47, 0x3F, 0x6A, 0x66, 0x42, 0xB2, 0x7E, 0x4A, 0xDB, 0xCD, 0xB6, 0xF8, 0x8E, 0xC3,			//	0 Dolphin=0x86840740=Zelda WW
	},
	{
		0x21, 0xD0, 0xC0, 0xEE, 0x25, 0x3D, 0x8C, 0x9E, 0x02, 0x58, 0x66, 0x7F, 0x3C, 0x1B, 0x11, 0xBC, 0x90, 0x1F, 0x33, 0xE2,			//	1 Dolphin=0x56d36052=Super Mario Sunshine
	},
	{
		0xBA, 0xDC, 0x60, 0x15, 0x33, 0x33, 0x28, 0xED, 0xB1, 0x0E, 0x72, 0xF2, 0x5B, 0x5A, 0xFB, 0xF3, 0xEF, 0x90, 0x30, 0x90,			//	2 Dolphin=0x4e8a8b21=Sonic Mega Collection
	},
	{
		0xBC, 0x17, 0x36, 0x81, 0x7A, 0x14, 0xB2, 0x1C, 0xCB, 0xF7, 0x3A, 0xD6, 0x8F, 0xDA, 0x57, 0xF8, 0x43, 0x78, 0x1A, 0xAE,			//	3 Dolphin=0xe2136399=Mario Party 5
	},
	{
		0xD9, 0x39, 0x63, 0xE3, 0x91, 0xD1, 0xA8, 0x5E, 0x4D, 0x5F, 0xD9, 0xC2, 0x9A, 0xF9, 0x3A, 0xA9, 0xAF, 0x8D, 0x4E, 0xF2,			//	4 Dolphin=0x07f88145=Zelda:OOT
	},
	{
		0xD8, 0x12, 0xAC, 0x09, 0xDC, 0x24, 0x50, 0x6B, 0x0D, 0x73, 0x3B, 0xF5, 0x39, 0x45, 0x1A, 0x23, 0x85, 0xF3, 0xC8, 0x79,			//	5 Dolphin=0x2fcdf1ec=Mario Kart DD
	},	
	{
		0x37, 0x44, 0xC3, 0x82, 0xC8, 0x98, 0x42, 0xD4, 0x9D, 0x68, 0x83, 0x1C, 0x2B, 0x06, 0x7E, 0xC7, 0xE8, 0x64, 0x32, 0x44,			//	6 Dolphin=0x3daf59b9=Star Fox Assault
	},
	{
		0xDA, 0x39, 0xA3, 0xEE, 0x5E, 0x6B, 0x4B, 0x0D, 0x32, 0x55, 0xBF, 0xEF, 0x95, 0x60, 0x18, 0x90, 0xAF, 0xD8, 0x07, 0x09,			//	7 0-Length DSP - Error
	},	 
	{
		0x8E, 0x5C, 0xCA, 0xEA, 0xA9, 0x84, 0x87, 0x02, 0xFB, 0x5C, 0x19, 0xD4, 0x18, 0x6E, 0xA7, 0x7B, 0xE5, 0xB8, 0x71, 0x78,			//	8 Dolphin=0x6CA33A6D=Donkey Kong Jungle Beat
	},	 
	{
		0xBC, 0x1E, 0x0A, 0x75, 0x09, 0xA6, 0x3E, 0x7C, 0xE6, 0x30, 0x44, 0xBE, 0xCC, 0x8D, 0x67, 0x1F, 0xA7, 0xC7, 0x44, 0xE5,			//	9 Dolphin=0x3ad3b7ac=Paper Mario The Thousand Year Door
	},	 
	{
		0x14, 0x93, 0x40, 0x30, 0x0D, 0x93, 0x24, 0xE3, 0xD3, 0xFE, 0x86, 0xA5, 0x68, 0x2F, 0x4C, 0x13, 0x38, 0x61, 0x31, 0x6C,			//	10 Dolphin=0x4be6a5cb=AC
	},	
	{
		0x09, 0xF1, 0x6B, 0x48, 0x57, 0x15, 0xEB, 0x3F, 0x67, 0x3E, 0x19, 0xEF, 0x7A, 0xCF, 0xE3, 0x60, 0x7D, 0x2E, 0x4F, 0x02,			//	11 Dolphin=0x42f64ac4=Luigi
	},
	{
		0x80, 0x01, 0x60, 0xDF, 0x89, 0x01, 0x9E, 0xE3, 0xE8, 0xF7, 0x47, 0x2C, 0xE0, 0x1F, 0xF6, 0x80, 0xE9, 0x85, 0xB0, 0x24,			//	12 Dolphin=0x267fd05a=Pikmin PAL
	},
	{
		0xB4, 0xCB, 0xC0, 0x0F, 0x51, 0x2C, 0xFE, 0xE5, 0xA4, 0xBA, 0x2A, 0x59, 0x60, 0x8A, 0xEB, 0x8C, 0x86, 0xC4, 0x61, 0x45,			//	13 Dolphin=0x6ba3b3ea=IPL NTSC 1.2
	},
	{
		0xA5, 0x13, 0x45, 0x90, 0x18, 0x30, 0x00, 0xB1, 0x34, 0x44, 0xAE, 0xDB, 0x61, 0xC5, 0x12, 0x0A, 0x72, 0x66, 0x07, 0xAA,			//	14 Dolphin=0x24b22038=IPL NTSC 1.0
	},
	{
		0x9F, 0x3C, 0x9F, 0x9E, 0x05, 0xC7, 0xD5, 0x0B, 0x38, 0x49, 0x2F, 0x2C, 0x68, 0x75, 0x30, 0xFD, 0xE8, 0x6F, 0x9B, 0xCA,			//	15 Dolphin=0x3389a79e=Metroid Prime Trilogy Wii (Needed?)
	},
};

const unsigned char DSPPattern[][0x10] =
{
	{
		0x02, 0x9F, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00,		//	0 Hash 12, 1, 0, 5, 8
	},
	{
		0x02, 0x9F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00,		//	1 Hash 14, 13, 11, 10
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0C, 0x10, 0x02, 0x9F, 0x0C, 0x1F, 0x02, 0x9F, 0x0C, 0x3B,		//	2 Hash 2
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x76, 0x02, 0x9F, 0x0E, 0x85, 0x02, 0x9F, 0x0E, 0xA1,		//	3 Hash 3
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0xB3, 0x02, 0x9F, 0x0E, 0xC2, 0x02, 0x9F, 0x0E, 0xDE,		//	4 Hash 4
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x71, 0x02, 0x9F, 0x0E, 0x80, 0x02, 0x9F, 0x0E, 0x9C,		//	5 Hash 9
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x88, 0x02, 0x9F, 0x0E, 0x97, 0x02, 0x9F, 0x0E, 0xB3,		//	6 Hash 6
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0D, 0xB0, 0x02, 0x9F, 0x0D, 0xBF, 0x02, 0x9F, 0x0D, 0xDB,		//	7 Hash 15
	},
};

typedef struct DspMatch
{
	u32 Length;
	u32 Pattern;
	u32 SHA1;
} DspMatch;

const DspMatch DspMatches[] =
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
	{ 0x000019E0,    2,      2 },
	{ 0x00001EC0,    3,      3 },
	{ 0x00001F20,    4,      4 },
	{ 0x00001EC0,    5,      9 },
	{ 0x00001F00,    6,      6 },
	{ 0x00001D40,    7,     15 },
};

#define AX_DSP_NO_DUP3 (0xFFFF)
void PatchAX_Dsp(u32 ptr, u32 Dup1, u32 Dup2, u32 Dup3, u32 Dup2Offset)
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

void PatchZelda_Dsp(u32 DspAddress, u32 PatchAddr, u32 OrigAddress, bool Split, bool KeepOrig)
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
void PatchZeldaInPlace_Dsp(u32 DspAddress, u32 OrigAddress)
{
	W32(DspAddress + (OrigAddress + 0) * 2, 0x009AFFFF); // lri $AX0.H, #0xffff
	W32(DspAddress + (OrigAddress + 2) * 2, 0x2AD62AD7); // srs @ACEAH, $AX0.H/srs @ACEAL, $AX0.H
	W32(DspAddress + (OrigAddress + 4) * 2, 0x02601000); // ori $AC0.M, 0x1000
}

void DoDSPPatch( char *ptr, u32 Version )
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
		default:
		{
		} break;
	}
}

static bool write32A( u32 Offset, u32 Value, u32 CurrentValue, u32 ShowAssert )
{
	if( read32(Offset) != CurrentValue )
	{
		#ifdef DEBUG_PATCH
		//if( ShowAssert)
			//dbgprintf("AssertFailed: Offset:%08X is:%08X should:%08X\r\n", Offset, read32(Offset), CurrentValue );
		#endif
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
	This offset gets randomly overwritten, this workaround fixes that problem.
*/
void Patch31A0( void )
{
	u32 PatchOffset = PATCH_OFFSET_START;

	u32 i;
	for (i = 0; i < (4 * 0x04); i+=0x04)
	{
		u32 Orig = *(vu32*)(0x319C + i);
		if ((Orig & 0xFC000002) == 0x40000000)
		{
			u32 NewAddr = (((s16)(Orig & 0xFFFC)) + 0x319C - PatchOffset);
			Orig = (Orig & 0xFFFF0003) | NewAddr;
#ifdef DEBUG_PATCH
			dbgprintf("Patch:[Patch31A0] applied (0x%08X), 0x%08X=0x%08X\r\n", 0x319C + i, PatchOffset + i, Orig);
#endif
		}
		else if ((Orig & 0xFC000002) == 0x48000000)
		{
			u32 BaseAddr = (Orig & 0x3FFFFFC);
			if(BaseAddr & 0x2000000) BaseAddr |= 0xFC000000;
			u32 NewAddr = (((s32)BaseAddr) + 0x319C - PatchOffset) & 0x3FFFFFC;
			Orig = (Orig & 0xFC000003) | NewAddr;
#ifdef DEBUG_PATCH
			dbgprintf("Patch:[Patch31A0] applied (0x%08X), 0x%08X=0x%08X\r\n", 0x319C + i, PatchOffset + i, Orig);
#endif
		}
		*(vu32*)(PatchOffset + i) = Orig;
	}

	PatchB( PatchOffset, 0x319C );
	PatchB( 0x31AC, PatchOffset+0x10 );
}

u32 PatchCopy(const u8 *PatchPtr, const u32 PatchSize)
{
	POffset -= PatchSize;
	memcpy( (void*)POffset, PatchPtr, PatchSize );
	return POffset;
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

		if( (op & 0xFC1FFFFF) == 0x3C00CC00 )	// lis rX, 0xCC00
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
				if (Datel && ((op & 0xFFFF) == 0x6000)) //Use original DI Status register to clear interrupts
					write32((u32)LISOff, LISOp | 0x0000CD80);
				else
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
				if (Datel && ((op & 0xFFFF) == 0x6000)) //Use original DI Status register to clear interrupts
					write32((u32)LISOff, LISOp | 0x0000CD80);
				else
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
					if (Datel && ((op & 0xFFFF) == 0x6000)) //Use original DI Status register to clear interrupts
						write32((u32)LISOff, LISOp | 0x0000CD80);
					else
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

#ifdef DEBUG_PATCH
	dbgprintf("Patch:[SI] applied %u times\r\n", SIPatched);
	if( DisableEXIPatch ) dbgprintf("Patch:[EXI] applied %u times\r\n", EXIPatched);
	dbgprintf("Patch:[AI] applied %u times\r\n", AIPatched);
	dbgprintf("Patch:[DI] applied %u times\r\n", DIPatched);
	#endif
}

u32 piReg = 0;
bool PatchProcessorInterface( u32 BufAt0, u32 Buffer )
{
	/* Pokemon XD - PI_FIFO_WP Direct */
	if(BufAt0 == 0x80033014 && (read32(Buffer+12) & 0xFC00FFFF) == 0x540037FE) //extrwi rZ, rY, 1,5
	{
		// Extract WRAPPED bit
		#ifdef DEBUG_PATCH
		u32 op = read32(Buffer+12);
		#else
		read32(Buffer+12);
		#endif
		W16(Buffer+14, 0x1FFE);
		#ifdef DEBUG_PATCH
		u32 src = (op >> 21) & 0x1F; //rY
		u32 dst = (op >> 16) & 0x1F; //rZ
		dbgprintf( "Patch:[PI_FIFO_WP PKM] extrwi r%i,r%i,1,2 (0x%08X)\r\n", dst, src, Buffer+12 );
		#endif
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
						#ifdef DEBUG_PATCH
						u32 lDst = (op >> 16) & 0x1F; //rZ
						dbgprintf( "Patch:[PI_FIFO_WP] rlwinm r%i,r%i,0,3,1 (0x%08X)\r\n", lDst, lSrc, piRegBuf+lPtr );
						#endif
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
						#ifdef DEBUG_PATCH
						u32 lDst = (op >> 16) & 0x1F; //rZ
						dbgprintf( "Patch:[PI_FIFO_WP] extrwi r%i,r%i,1,2 (0x%08X)\r\n", lDst, lSrc, piRegBuf+lPtr );
						#endif
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
						#ifdef DEBUG_PATCH
						sSrc = (read32(piRegBuf+sPtr) >> 21) & 0x1F;
						dbgprintf( "Patch:[PI_FIFO_WP] rlwinm r%i,r%i,0,3,1 (0x%08X)\r\n", sReg, sSrc, piRegBuf+sPtr );
						#endif
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

bool GameNeedsHook()
{
	if( (TITLE_ID) == 0x473258 )	// Sonic Gems Collection
		return (DOLSize != 1440100 && DOLSize != 1439812); //Everything except Sonic Fighters

	return( (TITLE_ID) == 0x474234 ||	// Burnout 2
			(TITLE_ID) == 0x47564A ||	// Viewtiful Joe
			(TITLE_ID) == 0x474146 ||	// Animal Crossing
			(TITLE_ID) == 0x475852 ||	// Mega Man X Command Mission
			(TITLE_ID) == 0x474832 ||	// NFS: HP2
			(TITLE_ID) == 0x474156 ||	// Avatar Last Airbender
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
			(GAME_ID) == 0x4747504A);	// SD Gundam Gashapon Wars
}

bool PADSwitchRequired()
{
	return( (TITLE_ID) == 0x47434F ||	// Call of Duty
			(TITLE_ID) == 0x475449 ||	// Tiger Woods PGA Tour 2003
			(TITLE_ID) == 0x475734 );	// Tiger Woods PGA Tour 2004
}

void MPattern(u8 *Data, u32 Length, FuncPattern *FunctionPattern)
{
	u32 i;

	memset32( FunctionPattern, 0, sizeof(FuncPattern) );

	for( i = 0; i < Length; i+=4 )
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

void SRAM_Checksum( unsigned short *buf, unsigned short *c1, unsigned short *c2) 
{
	u32 i;
	*c1 = 0; *c2 = 0;
	for (i = 0;i<4;++i)
	{
		*c1 += buf[0x06 + i];
		*c2 += (buf[0x06 + i] ^ 0xFFFF);
	}
	//dbgprintf("New Checksum: %04X %04X\r\n", *c1, *c2 );
}

char *getVidStr(u32 in)
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

#ifdef DEBUG_PATCH
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

int GotoFuncStart(int i, u32 Buffer)
{
	while( read32(Buffer + i - 4) != 0x4E800020 )
		i -= 4;
	return i;
}

int GotoFuncEnd(int i, u32 Buffer)
{
	do {
		i += 4; //known function, skip over it
	} while( read32(Buffer + i) != 0x4E800020 );
	return i;
}
void *OurBase = (void*)0x13002780;
void TRIWriteSettings(char *name, void *GameBase, u32 size)
{
	sync_before_read_align32(GameBase, size);
	sync_before_read_align32(OurBase, size);
	if((memcmp(OurBase, GameBase, size) != 0) && (memcmp(GameBase, "SB", 2) == 0))
	{
		dbgprintf("TRI:Writing Settings\r\n");
		memcpy(OurBase, GameBase, size);
		FIL backup;
		if(f_open_char(&backup, name, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK)
		{
			u32 wrote;
			f_write(&backup, OurBase, size, &wrote);
			f_close(&backup);
		}
		sync_after_write_align32(OurBase, size);
		TRI_BackupAvailable = 1;
	}
}
void TRIReadSettings(char *name, u32 size)
{
	FIL backup;
	if (f_open_char(&backup, name, FA_OPEN_EXISTING | FA_READ) == FR_OK)
	{
		if(backup.fsize == size)
		{
			dbgprintf("TRI:Reading Settings\r\n");
			u32 read;
			f_read(&backup, OurBase, size, &read);
			sync_after_write_align32(OurBase, size);
			TRI_BackupAvailable = 1;
		}
		f_close(&backup);
	}
}

void DoPatches( char *Buffer, u32 Length, u32 DiscOffset )
{
	if( (u32)Buffer == 0x01200000 && *(u8*)Buffer == 0x7C )
	{	/* Game can reset at any time */
		PatchState = PATCH_STATE_DONE;
		return;
	}

	int i, j, k;
	u32 read;
	u32 value = 0;

	// PSO 1&2 / III
	if (((TITLE_ID) == 0x47504F) || ((TITLE_ID) == 0x475053))
	{
		if((PSOHack & PSO_STATE_SWITCH) && DiscOffset > 0)
		{
			#ifdef DEBUG_PATCH
			dbgprintf("PSO:psov3.dol\r\n");
			#endif
			PSOHack = PSO_STATE_LOAD | PSO_STATE_NOENTRY;
		}
		if(Length == 0x318E0 && read32((u32)Buffer+0x318B0) == 0x4CBEBC20)
		{
			#ifdef DEBUG_PATCH
			dbgprintf("PSO:switcher.dol\r\n");
			#endif
			PSOHack = PSO_STATE_LOAD | PSO_STATE_SWITCH;
		}
		else if(Length == 0x1B8A0 && read32((u32)Buffer+0x12E0C) == 0x7FA4E378)
		{
			#ifdef DEBUG_PATCH
			dbgprintf("PSO:switcherD.dol\r\n");
			#endif
			PSOHack = PSO_STATE_LOAD | PSO_STATE_SWITCH;
		}
		else if((Length == 0x19580 && read32((u32)Buffer+0x19560) == 0xC8BFAF78) || //PSO Plus
				(Length == 0x19EA0 && read32((u32)Buffer+0x19E80) == 0x24C7E996) || //PSO 3 PAL
				(Length == 0x1A2C0 && read32((u32)Buffer+0x1A2A0) == 0xE2BEE1FF))   //PSO 3 NTSC
		{
			#ifdef DEBUG_PATCH
			dbgprintf("PSO:switcher.prs\r\n");
			#endif
			PSOHack = PSO_STATE_LOAD | PSO_STATE_PSR | PSO_STATE_SWITCH;
		}
	}

	if ((TITLE_ID) == 0x474E48)  //NHL Hitz = Possible Datel
	{
		if (Datel)
		{
			switch ((u32)Buffer)
			{
				case 0x01300000:	// Datel AppLoader
				{
				} break;
				case 0x00003100:	// Datel DOL
				{
#ifdef DEBUG_PATCH
					dbgprintf("Patch:Datel loading dol:0x%p %u\r\n", Buffer, Length);
#endif
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
#ifdef DEBUG_DI
				dbgprintf("DIP:DOL Size:%d MinOff:0x%08X MaxOff:0x%08X\r\n", DOLSize, DOLMinOff, DOLMaxOff );
#endif
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
#ifdef DEBUG_DI
			dbgprintf("DIP:ELF Size:%d MinOff:0x%08X MaxOff:0x%08X\r\n", DOLSize, DOLMinOff, DOLMaxOff );
#endif
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
			u32 NewVal = (PATCH_OFFSET_ENTRY - P2C(GameEntry));
			NewVal &= 0x03FFFFFC;
			NewVal |= 0x48000000;
			FirstLine = read32((u32)Buffer+0xB4);
			write32((u32)Buffer+0xB4, NewVal);
			dbgprintf("DIP:Hacked ELF FirstLine:0x%08X MinOff:0x%08X MaxOff:0x%08X\r\n", FirstLine, DOLMinOff, DOLMaxOff );
			PatchState |= PATCH_STATE_LOAD;
		}
	}

	if( PatchState == PATCH_STATE_DONE && (u32)Buffer == 0x01300000 && *(vu8*)Buffer == 0x48 )
	{	/* Make sure to force patch that area */
		PatchState = PATCH_STATE_PATCH;
		DOLSize = Length;
		DOLMinOff = (u32)Buffer;
		DOLMaxOff = DOLMinOff + Length;
		/* Make sure to backup ingame settings */
		if(TRIGame == TRI_VS4)
			TRIWriteSettings("/saves/VS4settings.bin", (void*)0x69E4E0, 0x2B);
		else if(TRIGame == TRI_AX)
			TRIWriteSettings("/saves/AXsettings.bin", (void*)0x3CFBD0, 0x2B);
	}

	if( TRIGame == TRI_SB && Length == 0x1C0 && *((vu8*)Buffer+0x0F) == 0x06 )
	{	/* Fixes Caution 51 */
		*((vu8*)Buffer+0x0F) = 0x07;
		dbgprintf("TRI:Patched boot.id Video Mode\r\n");
	}

	/* Modify Sonic Riders _Main.rel not crash on certain mem copies */
	if( (GAME_ID) == 0x47584545 && (u32)Buffer == SONICRIDERS_BASE_NTSCU
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
	}

	if (!(PatchState & PATCH_STATE_PATCH))
		return;
	piReg = 0;

	sync_after_write(Buffer, Length);

	Buffer = (char*)DOLMinOff;
	Length = DOLMaxOff - DOLMinOff;

#ifdef DEBUG_PATCH
	dbgprintf("Patch:Offset:0x%08X EOffset:0x%08X Length:%08X\r\n", Buffer, DOLMaxOff, Length );
	dbgprintf("Patch:Game ID = %x\r\n", read32(0));
#endif

	sync_before_read(Buffer, Length);

	/* Have gc patches loaded at the same place for reloading games */
	POffset = PATCH_OFFSET_ENTRY;

	u32 __DVDInterruptHandlerAddr = PatchCopy(__DVDInterruptHandler, __DVDInterruptHandler_size);
	//Combine these?  Worried if an interrupt occurs while the code is being overwritten
	u32 __DVDInterruptHandlerAddr2 = PatchCopy(__DVDInterruptHandler, __DVDInterruptHandler_size);

	u32 FakeInterruptAddr = PatchCopy(FakeInterrupt, FakeInterrupt_size);
	u32 FakeInterrupt_DBGAddr = PatchCopy(FakeInterrupt_DBG, FakeInterrupt_DBG_size);
	u32 TCIntrruptHandlerAddr = PatchCopy(TCIntrruptHandler, TCIntrruptHandler_size);
	u32 SIIntrruptHandlerAddr = PatchCopy(SIIntrruptHandler, SIIntrruptHandler_size);

	u32 SIInitStoreAddr = PatchCopy(SIInitStore, SIInitStore_size);
	u32 FakeRSWLoadAddr = PatchCopy(FakeRSWLoad, FakeRSWLoad_size);
	u32 FakeRSWStoreAddr = PatchCopy(FakeRSWStore, FakeRSWStore_size);

	u32 AIInitDMAAddr = PatchCopy(AIInitDMA, AIInitDMA_size);
	u32 __DSPHandlerAddr = PatchCopy(__DSPHandler, __DSPHandler_size);
	u32 __ARHandlerAddr = PatchCopy(__ARHandler, __ARHandler_size);

	//if (((TITLE_ID) == 0x47504F) || ((TITLE_ID) == 0x475053))  // Make these FuncPatterns?
	u32 SwitcherPrsAddr = PatchCopy(SwitcherPrs, SwitcherPrs_size);

	u32 DatelTimerAddr = PatchCopy(DatelTimer, DatelTimer_size);

	if(( TITLE_ID == 0x475858 ) || ( TITLE_ID == 0x474336 ))
	{
		// patch out initial memset(0x1800, 0, 0x1800)
		if( (read32(0) & 0xFF) == 0x4A )	// JAP
			write32( 0x560C, 0x60000000 );
		else								// EUR/USA
			write32( 0x5614, 0x60000000 );

		// patch memset to jump to test function
		write32(0x00005498, 0x4BFFABF0);

		// patch in test < 0x3000 function
		write32(0x00000088, 0x3D008000);
		write32(0x0000008C, 0x61083000);
		write32(0x00000090, 0x7C044000);
		write32(0x00000094, 0x4180542C);
		write32(0x00000098, 0x90E40004);
		write32(0x0000009C, 0x48005400);

		// skips __start init of debugger mem
		write32(0x00003194, 0x48000028);
	}

	if( read32(0x023D240) == 0x386000A8 )
	{
		dbgprintf("TRI:Mario Kart GP1\r\n");
		TRIGame = TRI_GP1;
		SystemRegion = REGION_JAPAN;

		//Unlimited CARD uses
		write32( 0x01F5C44, 0x60000000 );

		//Disable cam
		write32( 0x00790A0, 0x98650025 );

		//Disable CARD
		//write32( 0x00790B4, 0x98650023 );
		//write32( 0x00790CC, 0x98650023 );

		//Disable wheel/handle
		write32( 0x007909C, 0x98650022 );

		//VS wait
		write32( 0x00BE10C, 0x4800002C );

		//cam loop
		write32( 0x009F1E0, 0x60000000 );

		//Enter test mode (anti-freeze)
		write32( 0x0031BF0, 0x60000000 );
		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenu, CheckTestMenu_size), 0x31BF4 );

		//Remove some menu timers
		write32( 0x0019BFF8, 0x60000000 ); //card check
		write32( 0x001BCAA8, 0x60000000 ); //want to make a card
		write32( 0x00195748, 0x60000000 ); //card view
		write32( 0x000CFABC, 0x60000000 ); //select game mode
		write32( 0x000D9F14, 0x60000000 ); //select character
		write32( 0x001A8CF8, 0x60000000 ); //select cup
		write32( 0x001A89B8, 0x60000000 ); //select round
		write32( 0x001A36CC, 0x60000000 ); //select item pack (card)
		write32( 0x001A2A10, 0x60000000 ); //select item (card)
		write32( 0x001B9724, 0x60000000 ); //continue
		write32( 0x001E61B4, 0x60000000 ); //rewrite rank
		write32( 0x001A822C, 0x60000000 ); //select course p1 (time attack)
		write32( 0x001A7F0C, 0x60000000 ); //select course p2 (time attack)
		write32( 0x000D6234, 0x60000000 ); //enter name (time attack, card)
		write32( 0x001BF1DC, 0x60000000 ); //save record p1 (time attack on card)
		write32( 0x001BF1DC, 0x60000000 ); //save record p2 (time attack on card)
		write32( 0x000E01B4, 0x60000000 ); //select record place (time attack on card)

		//Modify to regular GX pattern to patch later
		write32( 0x363660, 0x00 ); //NTSC Interlaced

		//Test Menu Interlaced
		//memcpy( (void*)0x036369C, (void*)0x040EB88, 0x3C );

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadGP, PADReadGP_size), 0x3C2A0 );

		//some report check skip
		//write32( 0x00307CC, 0x60000000 );

		//memcpy( (void*)0x00330BC, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x0030710, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x0030760, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x020CBCC, OSReportDM, sizeof(OSReportDM) );	// UNkReport
		//memcpy( (void*)0x02281B8, OSReportDM, sizeof(OSReportDM) );	// UNkReport4
	}
	else if (read32(0x02856EC) == 0x386000A8)
	{
		dbgprintf("TRI:Mario Kart GP2\r\n");
		TRIGame = TRI_GP2;
		SystemRegion = REGION_JAPAN;

		//Unlimited CARD uses
		write32( 0x00A02F8, 0x60000000 );

		//Enter test mode (anti-freeze)
		write32( 0x002E340, 0x60000000 );
		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenu, CheckTestMenu_size), 0x2E344 );

		//Disable wheel
		write32( 0x007909C, 0x98650022 );

		//Disable CARD
		//write32( 0x0073BF4, 0x98650023 );
		//write32( 0x0073C10, 0x98650023 );

		//Disable cam
		write32( 0x0073BD8, 0x98650025 );

		//Disable red item button
		write32( 0x0073C98, 0x98BF0045 );

		//VS wait patch 
		write32( 0x0084FC4, 0x4800000C );
		write32( 0x0085000, 0x60000000 );

		//Game vol
		//write32( 0x00790E8, 0x39000009 );

		//Attract vol
		//write32( 0x00790F4, 0x38C0000C );

		//Disable Commentary (sets volume to 0 )
		//write32( 0x001B6510, 0x38800000 );

		//Patches the analog input count
		write32( 0x000392F4, 0x38000003 );

		//Remove some menu timers (thanks conanac)
		write32( 0x001C1074, 0x60000000 ); //card check
		write32( 0x001C0174, 0x60000000 ); //want to make a card
		write32( 0x00232818, 0x60000000 ); //card view
		write32( 0x001C1A08, 0x60000000 ); //select game mode
		write32( 0x001C3380, 0x60000000 ); //select character
		write32( 0x001D7594, 0x60000000 ); //select kart
		write32( 0x001C7A7C, 0x60000000 ); //select cup
		write32( 0x001C9ED8, 0x60000000 ); //select round
		write32( 0x00237B3C, 0x60000000 ); //select item (card)
		write32( 0x0024D35C, 0x60000000 ); //continue
		write32( 0x0015F2F4, 0x60000000 ); //rewrite rank
		write32( 0x001CF5DC, 0x60000000 ); //select course (time attack)
		write32( 0x001BE248, 0x60000000 ); //enter name (time attack, card)
		write32( 0x0021DFF0, 0x60000000 ); //save record (time attack on card)

		//Make some menu timers invisible
		write32( 0x001B7D2C, 0x60000000 );
		write32( 0x00231CA0, 0x60000000 );

		//Modify to regular GX pattern to patch later
		write32( 0x3F1FD0, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadGP, PADReadGP_size), 0x38A34 );

		//memcpy( (void*)0x002CE3C, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x002CE8C, OSReportDM, sizeof(OSReportDM) );
		//write32( 0x002CEF8, 0x60000000 );
	}
	else if( read32(0x01851C4) == 0x386000A8 )
	{
		dbgprintf("TRI:F-Zero AX\r\n");
		TRIGame = TRI_AX;
		SystemRegion = REGION_JAPAN;
		DISetDIMMVersion(0x12301777);

		//Reset loop
		write32( 0x01B5410, 0x60000000 );

		//DBGRead fix
		write32( 0x01BEF38, 0x60000000 );

		//Disable CARD
		//write32( 0x017B2BC, 0x38C00000 );

		//Motor init patch
		write32( 0x0175710, 0x60000000 );
		write32( 0x0175714, 0x60000000 );
		write32( 0x01756AC, 0x60000000 );

		//patching system waiting stuff
		write32( 0x0180DB8, 0x48000054 );
		write32( 0x0180E1C, 0x48000054 );

		//Network waiting
		write32( 0x0180FD8, 0x4800004C );

		//Goto Test menu
		write32( 0x00DF3D0, 0x60000000 );
		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenu, CheckTestMenu_size), 0xDF3D4 );

		//Replace Motor Init with Controller Setup
		write32( 0x0024E034, 0x80180D5C );
		write32( 0x0024E038, 0x80180BC0 );

		//English
		write32( 0x000DF698, 0x38000000 );

		//Remove some menu timers (thanks dj_skual)
		// (menu gets constantly removed in JVSIO.c)
		write32( 0x0015B148, 0x60000000 ); //after race

		//Make some menu timers invisible (thanks dj_skual)
		write32( 0x0023759C, 0x40200000 ); //menu inner
		write32( 0x002375D4, 0x40200000 ); //menu outer
		write32( 0x00237334, 0x00000000 ); //after race inner
		write32( 0x0023736C, 0x00000000 ); //after race outer

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings("/saves/AXsettings.bin", 0x2B);
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsAX, RestoreSettingsAX_size), 0x17B490);

		//Modify to regular GX pattern to patch later
		u32 NTSC480ProgTri = 0x21D3EC;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		NTSC480ProgTri = 0x302AC0;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		//PAD Hook for control updates
		PatchBL( PatchCopy(PADReadF, PADReadF_size), 0x1B4368 );

		//memcpy( (void*)0x01CAACC, patch_fwrite_GC, sizeof(patch_fwrite_GC) );
		//memcpy( (void*)0x01882C0, OSReportDM, sizeof(OSReportDM) );

		//Unkreport
		//PatchB( 0x01882C0, 0x0191B54 );
		//PatchB( 0x01882C0, 0x01C53CC );
		//PatchB( 0x01882C0, 0x01CC684 );
	}
	else if( read32( 0x0210C08 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 4 Ver 2006 (EXPORT)\r\n");
		TRIGame = TRI_VS4;
		SystemRegion = REGION_USA;
		DISetDIMMVersion(0xA3A479);

		//BOOT/FIRM version mismatch patch (not needed with SegaBoot)
		write32( 0x0051924, 0x60000000 );

		//Set menu timer to about 51 days
		write32( 0x00CBB7C, 0x3C800FFF );

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x3B804 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings("/saves/VS4settings.bin", 0x2B);
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS, RestoreSettingsVS_size), 0x12BD4);

		//Modify to regular GX pattern to patch later
		write32( 0x26DD38, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS, PADReadVS_size), 0x3C504 );

		//memcpy( (void*)0x001C2B80, OSReportDM, sizeof(OSReportDM) );
	}
	else if(useipltri)
	{
		if( read32( 0x00055F98 ) == 0x386000A8 )
		{
			write32(0x000627A4, 0x3C808006); //lis r4, 0x8006
			write32(0x000627AC, 0x6084E93C); //ori r4, r4, 0xE93C
			write32(0x0006E93C, 0x01010A01); //modify 0x8006E93C for Free Play
			dbgprintf("TRI:SegaBoot (Patched Free Play)\r\n");
		}
		else
			dbgprintf("TRI:SegaBoot\r\n");
		TRIGame = TRI_SB;
	}

	DisableEXIPatch = (TRIGame == TRI_NONE && ConfigGetConfig(NIN_CFG_MEMCARDEMU) == false);
	DisableSIPatch = (TRIGame == TRI_NONE && ConfigGetConfig(NIN_CFG_NATIVE_SI));

	/* Triforce relevant function */
	u32 GCAMSendCommandAddr = TRIGame ? PatchCopy(GCAMSendCommand, sizeof(GCAMSendCommand)) : 0;

	bool PatchWide = ConfigGetConfig(NIN_CFG_FORCE_WIDE);
	if(PatchWide && PatchStaticWidescreen(TITLE_ID, GAME_ID & 0xFF)) //if further patching is needed
		PatchWide = false;

	PatchFuncInterface( Buffer, Length );
	sync_after_write( Buffer, Length );
	sync_before_read( Buffer, Length );

	u32 PatchCount = FPATCH_OSSleepThread | FPATCH_VideoModes;
#ifdef CHEATS
	if( IsWiiU )
	{
		if( ConfigGetConfig(NIN_CFG_CHEATS) )
			PatchCount &= ~FPATCH_OSSleepThread;
	}
	else
	{
		if( ConfigGetConfig(NIN_CFG_DEBUGGER|NIN_CFG_CHEATS) )
			PatchCount &= ~FPATCH_OSSleepThread;
	}
#endif
	if( ConfigGetConfig(NIN_CFG_FORCE_PROG) || (ConfigGetVideoMode() & NIN_VID_FORCE) )
		PatchCount &= ~FPATCH_VideoModes;

	u32 patitr;
	for(patitr = 0; patitr < PCODE_MAX; ++patitr)
	{
		FuncPattern *CurPatterns = AllFPatterns[patitr].pat;
		u32 CurPatternsLen = AllFPatterns[patitr].patlen;
		for( j=0; j < CurPatternsLen; ++j )
			CurPatterns[j].Found = 0;
	}
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
			if(PatchProcessorInterface(BufAt0, (u32)Buffer + i) || PatchTimers(BufAt0, (u32)Buffer + i))
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
			}
			if( (PatchCount & FPATCH_OSDispatchIntr) == 0 )
			{
				if( BufAt0 == 0x3C60CC00 &&
					BufAt4 == 0x83E33000)
				{
					printpatchfound("__OSDispatchInterrupt", NULL, (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					PatchBL( FakeInterruptAddr, (u32)Buffer + i + 4 );
					if(DisableEXIPatch == 0)
					{
						// EXI Device 0 Control Register
						write32A( (u32)Buffer+i+0x114, 0x3C60C000, 0x3C60CC00, 1 );
						write32A( (u32)Buffer+i+0x118, 0x80830010, 0x80836800, 1 );
						if(TRIGame)
						{
							// EXI Device 2 Control Register (Trifroce)
							write32A( (u32)Buffer+i+0x188, 0x3C60C000, 0x3C60CC00, 1 );
							write32A( (u32)Buffer+i+0x18C, 0x38630018, 0x38636800, 1 );
							write32A( (u32)Buffer+i+0x190, 0x80830000, 0x80830028, 1 );
						}
					}
					PatchCount |= FPATCH_OSDispatchIntr;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
				else if( BufAt0 == 0x3C60CC00 && BufAt4 == 0x83A33000 && read32( (u32)Buffer + i + 8 ) == 0x57BD041C)
				{
					printpatchfound("__OSDispatchInterrupt", "DBG", (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					PatchBL( FakeInterrupt_DBGAddr, (u32)Buffer + i + 4 );
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
					#ifdef DEBUG_PATCH
					dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
					#endif
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
					#ifdef DEBUG_PATCH
					dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
					#endif
					//PatchDiscInterface( (char*)Offset );
					u32 OrigData = read32(HwOffset);
					PatchBL(__DVDInterruptHandlerAddr, HwOffset);
					if ((read32(__DVDInterruptHandlerAddr + __DVDInterruptHandler_size - 0x8) & 0xFC00FFFF) == 0x3C00D302)
					{
						write32(__DVDInterruptHandlerAddr + __DVDInterruptHandler_size - 0x8, OrigData);
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", OrigData, HwOffset);
						#endif
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
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
						#endif
						// Save r0 (lr) before bl
						u32 OrigData = read32(HwOffset);
						write32(HwOffset, read32(HwOffset + 4));
						write32(HwOffset + 4, OrigData);
						HwOffset = Offset + 4;
					}
					#ifdef DEBUG_PATCH
					dbgprintf("Patch:[__DVDInterruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
					#endif
					//PatchDiscInterface( (char*)Offset );
					u32 OrigData = read32(HwOffset);
					PatchBL(__DVDInterruptHandlerAddr2, HwOffset);
					if ((read32(__DVDInterruptHandlerAddr2 + __DVDInterruptHandler_size - 0x8) & 0xFC00FFFF) == 0x3C00D302)
					{
						write32(__DVDInterruptHandlerAddr2 + __DVDInterruptHandler_size - 0x8, OrigData);
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
							#ifdef DEBUG_PATCH
							dbgprintf("Patch:[GXInit] stw r5,-0x%X(r13) (0x%08X)\r\n", 0x10000 - piReg, (u32)Buffer + j + 2);
							#endif
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
							#ifdef DEBUG_PATCH
							dbgprintf("Patch:[GXInit DBG] stw r3,-0x%X(r13) (0x%08X)\r\n", 0x10000 - piReg, (u32)Buffer + j + 2);
							#endif
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
					printpatchfound("__CARDUnlock", NULL, (u32)Buffer + GotoFuncStart(i, (u32)Buffer));
					write32( (u32)Buffer+i, 0x3CC01000 ); //lis r6, 0x1000
					write32( (u32)Buffer+i+28, 0x909D0004 ); //stw r4, 4(r29)
					write32( (u32)Buffer+i+36, 0x90DD0008 ); //stw r6, 8(r29)
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
	#ifdef CHEATS
			if( (PatchCount & FPATCH_OSSleepThread) == 0 )
			{
				//OSSleepThread(Pattern 1)
				if( BufAt0 == 0x3C808000 &&
					( BufAt4 == 0x3C808000 || BufAt4 == 0x808400E4 ) &&
					( read32((u32)Buffer + i + 8 ) == 0x38000004 || read32((u32)Buffer + i + 8 ) == 0x808400E4 ) )
				{
					int j = 12;

					while( *(vu32*)(Buffer+i+j) != 0x4E800020 )
						j+=4;

					u32 DebuggerHook = (u32)Buffer + i + j;
					printpatchfound("Hook:OSSleepThread",NULL, DebuggerHook);

					u32 DBGSize;

					FIL fs;
					if( f_open_char( &fs, "/sneek/kenobiwii.bin", FA_OPEN_EXISTING|FA_READ ) != FR_OK )
					{
						#ifdef DEBUG_PATCH
						dbgprintf( "Patch:Could not open:\"%s\", this file is required for debugging!\r\n", "/sneek/kenobiwii.bin" );
						#endif
					}
					else
					{
						if( fs.fsize != 0 )
						{
							DBGSize = fs.fsize;

							//Read file to memory
							s32 ret = f_read( &fs, (void*)0x1800, fs.fsize, &read );
							if( ret != FR_OK )
							{
								#ifdef DEBUG_PATCH
								dbgprintf( "Patch:Could not read:\"%s\":%d\r\n", "/sneek/kenobiwii.bin", ret );
								#endif
								f_close( &fs );
							}
							else
							{
								f_close( &fs );

								if( IsWiiU )
								{
									*(vu32*)(P2C(*(vu32*)0x1808)) = 0;
								}
								else
								{
									if( ConfigGetConfig(NIN_CFG_DEBUGWAIT) )
										*(vu32*)(P2C(*(vu32*)0x1808)) = 1;
									else
										*(vu32*)(P2C(*(vu32*)0x1808)) = 0;
								}

								memcpy( (void *)0x1800, (void*)0, 6 );

								u32 newval = 0x18A8 - DebuggerHook;
									newval&= 0x03FFFFFC;
									newval|= 0x48000000;

								*(vu32*)(DebuggerHook) = newval;

								if( ConfigGetConfig( NIN_CFG_CHEATS ) )
								{
									char *path = (char*)malloc( 128 );

									if( ConfigGetConfig(NIN_CFG_CHEAT_PATH) )
									{
										_sprintf( path, "%s", ConfigGetCheatPath() );
									} else {
										_sprintf( path, "/games/%.6s/%.6s.gct", (char*)0x1800, (char*)0x1800 );
									}

									FIL CodeFD;
									u32 read;

									if( f_open_char( &CodeFD, path, FA_OPEN_EXISTING|FA_READ ) == FR_OK )
									{
										if( CodeFD.fsize >= 0x2E60 - (0x1800+DBGSize-8) )
										{
											#ifdef DEBUG_PATCH
											dbgprintf("Patch:Cheatfile is too large, it must not be large than %d bytes!\r\n", 0x2E60 - (0x1800+DBGSize-8));
											#endif
										} else {
											if( f_read( &CodeFD, (void*)(0x1800+DBGSize-8), CodeFD.fsize, &read ) == FR_OK )
											{
												#ifdef DEBUG_PATCH
												dbgprintf("Patch:Copied cheat file to memory\r\n");
												#endif
												write32( 0x1804, 1 );
											} else {
												#ifdef DEBUG_PATCH
												dbgprintf("Patch:Failed to read cheat file:\"%s\"\r\n", path );
												#endif
											}
										}

										f_close( &CodeFD );

									} else {
										#ifdef DEBUG_PATCH
										dbgprintf("Patch:Failed to open/find cheat file:\"%s\"\r\n", path );
										#endif
									}

									free(path);
								}
							}
						}
					}

					PatchCount |= FPATCH_OSSleepThread;
					i = GotoFuncEnd(i, (u32)Buffer);
					continue;
				}
			}
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
								printvidpatch(VI_PAL, VI_480P, (u32)Buffer+i);
								write32( (u32)Buffer+i, 0x02 );
								//write32( (u32)Buffer+i, 0x06 );
								write32( (u32)Buffer+i+0x14, 0); //mode sf
								//memcpy(Buffer+i, GXNtsc480Prog, sizeof(GXNtsc480Prog));
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
					else
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
								if(NinForceMode == NIN_VID_FORCE_PAL50 || NinForceMode == NIN_VID_FORCE_PAL60)
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
				u32 l;
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

							#ifdef DEBUG_PATCH
							dbgprintf("Patch:[DSP v%u] patched (0x%08X)\r\n", Known, Buffer + i );
							#endif
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

		FuncPattern fp;
		MPattern( (u8*)(Buffer+i), Length, &fp );
		//if ((((u32)Buffer + i) & 0x7FFFFFFF) == 0x00000000) //(FuncPrint)
		//	dbgprintf("FuncPattern: 0x%X, %d, %d, %d, %d, %d\r\n", fp.Length, fp.Loads, fp.Stores, fp.FCalls, fp.Branch, fp.Moves);
		for(patitr = 0; patitr < PCODE_MAX; ++patitr)
		{
			if(AllFPatterns[patitr].patmode == PCODE_TRI && TRIGame == TRI_NONE)
				continue;
			if(AllFPatterns[patitr].patmode == PCODE_EXI && DisableEXIPatch)
				continue;
			if (AllFPatterns[patitr].patmode == PCODE_DATEL && Datel == 0)
				continue;
			if (AllFPatterns[patitr].patmode == PCODE_SI && DisableSIPatch)
				continue;
			FuncPattern *CurPatterns = AllFPatterns[patitr].pat;
			u32 CurPatternsLen = AllFPatterns[patitr].patlen;
			bool patfound = false;
			for( j=0; j < CurPatternsLen; ++j )
			{
				if( CurPatterns[j].Found ) //Skip already found patches
					continue;

				if( CPattern( &fp, &(CurPatterns[j]) ) )
				{
					u32 FOffset = (u32)Buffer + i;

					CurPatterns[j].Found = FOffset;
					//#ifdef DEBUG_PATCH
					//dbgprintf("Patch:[%s] found (0x%08X)\r\n", CurPatterns[j].Name, FOffset );
					//#endif

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
							if( read32( FOffset+0x11C ) == 0x5400023E )
							{
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x11C);
								write32( FOffset+0x11C, 0x5400033E );
							}
							else if( read32( FOffset+0x34 ) == 0x5400023E )
							{
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x34);
								write32( FOffset+0x34, 0x5400033E );
							}
							else if( read32( FOffset+0x28 ) == 0x5004C00E )
							{
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x28);
								write32( FOffset+0x28, 0x5004C016 );
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
									PatchBL(__ARHandlerAddr, FOffset + 0x44);
								}
								else
								{
									#ifdef DEBUG_PATCH
									dbgprintf("Patch:[__ARHandler] skipped (0x%08X)\r\n", FOffset);
									#endif
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
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							memcpy( (void*)FOffset, __OSReadROM, sizeof(__OSReadROM) );
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
						case FCODE___GXSetVAT:	// Patch for __GXSetVAT, fixes the dungeon map freeze in Wind Waker
						{
							if(useipl == 1) break;
							switch( TITLE_ID )
							{
								case 0x505A4C:	// The Legend of Zelda: Collector's Edition
									if( !(DOLSize == 3847012 || DOLSize == 3803812 || DOLSize == 3763812) )
										break;	// only patch the main.dol of the Zelda:ww game
								case 0x475A4C:	// The Legend of Zelda: The Wind Waker
								case 0x475352:	// Smugglers Run: Warezones
								{
									u32 tmp1 = read32(FOffset) & 0x1FFFFF;
									u32 tmp2 = read32(FOffset+0x18) & 0xFFFF;
									memcpy( (void*)FOffset, __GXSetVAT, __GXSetVAT_size );
									write32(FOffset, (read32(FOffset) & 0xFFE00000) | tmp1);
									write32(FOffset+0x8, (read32(FOffset+0x8) & 0xFFFF0000) | tmp2);
									write32(FOffset+0x78, (read32(FOffset+0x78) & 0xFFFF0000) | tmp2);
									printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								} break;
								default:
								break;
							}
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
							PatchBL( TCIntrruptHandlerAddr, (FOffset + PatchOffset) );
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + PatchOffset);
						} break;
						case FCODE_EXIDMA:	// EXIDMA
						{
							u32 off		= 0;
							s32 reg		=-1;
							u32 valueB	= 0;

							while(1)
							{
								off += 4;

								u32 op = read32( (u32)FOffset + off );

								if( reg == -1 )
								{
									if( (op & 0xFC1F0000) == 0x3C000000 )	// lis rX, 0xCC00
									{
										reg = (op & 0x3E00000) >> 21;
										if( reg == 3 )
										{
											value = read32( FOffset + off ) & 0x0000FFFF;
											#ifdef DEBUG_PATCH
											//dbgprintf("lis:%08X value:%04X\r\n", FOffset+off, value );
											#endif
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
												#ifdef DEBUG_PATCH
												//dbgprintf("addi:%08X value:%04X\r\n", FOffset+off, valueB);
												#endif
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
							W16(FOffset + 6, valueB + 4);
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
								#ifdef DEBUG_PATCH
								dbgprintf("Patch:[ARInit] skipped (0x%08X)\r\n", FOffset);
								#endif
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
									 (TITLE_ID) == 0x443737 )	// Multi-Game Demo Disc 18
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
						case FCODE_GCAMSendCommand:
						{
							if(read32(FOffset + 0x2C) == 0x38C40080)
							{
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
								/* Data I/O */
								W16(GCAMSendCommandAddr+0x0E, R16(FOffset+0x1E));
								W16(GCAMSendCommandAddr+0x12, R16(FOffset+0x22));
								/* Callback */
								W16(GCAMSendCommandAddr+0x9A, R16(FOffset+0x26));
								W16(GCAMSendCommandAddr+0x9E, R16(FOffset+0x2A));
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
								#ifdef DEBUG_PATCH
								dbgprintf("Patch:[patch_fwrite_Log] skipped (0x%08X)\r\n", FOffset);
								#endif
								break;
							}

							if( IsWiiU )
							{
								if( CurPatterns[j].Patch == patch_fwrite_GC ) // patch_fwrite_Log works fine
								{
									#ifdef DEBUG_PATCH
									dbgprintf("Patch:[patch_fwrite_GC] skipped (0x%08X)\r\n", FOffset);
									#endif
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
											PatchBL( SIIntrruptHandlerAddr, (SIBase + PatchOffset) );
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
						} break;
						case FCODE_PADControlAllMotors:
						{
							u32 CheckOffset = CurPatterns[j].Length - 0x24;
							if((read32(FOffset+CheckOffset) & 0xFC00FFFF) != 0x2C000000) //cmpwi rX, 0
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							memcpy((void*)FOffset, PADControlAllMotors, PADControlAllMotors_size);
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE_PADControlMotor:
						{
							if((read32(FOffset+0x24) & 0xFC00FFFF) != 0x3C008000) //lis rX, 0x8000
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							memcpy((void*)FOffset, PADControlMotor, PADControlMotor_size);
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE_PADIsBarrel:
						{
							if(memcmp((void*)(FOffset), PADIsBarrelOri, sizeof(PADIsBarrelOri)) != 0)
							{
								CurPatterns[j].Found = 0; // False hit
								break;
							}
							memcpy((void*)(FOffset), PADIsBarrel, PADIsBarrel_size);
							printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
						} break;
						case FCODE___OSResetHandler:
						{
							if(read32(FOffset + 0x84) == 0x540003DF)
							{
								PatchBL( FakeRSWLoadAddr, (FOffset + 0x84) );
								PatchBL( FakeRSWLoadAddr, (FOffset + 0x94) );
								PatchBL( FakeRSWStoreAddr, (FOffset + 0xD4) );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x84);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_OSGetResetState:
						{
							if(read32(FOffset + 0x28) == 0x540003DF) /* Patch C */
							{
								PatchBL( FakeRSWLoadAddr, (FOffset + 0x28) );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x28);
							}
							else if(read32(FOffset + 0x2C) == 0x540003DF) /* Patch A, B */
							{
								PatchBL( FakeRSWLoadAddr, (FOffset + 0x2C) );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset + 0x2C);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE_AIInitDMA:
						{
							if(read32(FOffset + 0x20) == 0x3C80CC00)
							{
								PatchBL( AIInitDMAAddr, (FOffset + 0x20) );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
							}
							else
								CurPatterns[j].Found = 0;
						} break;
						case FCODE___DSPHandler:
						{
							if(useipl == 1) break;
							if(read32(FOffset + 0xF8) == 0x2C000000)
							{
								PatchBL( __DSPHandlerAddr, (FOffset + 0xF8) );
								printpatchfound(CurPatterns[j].Name, CurPatterns[j].Type, FOffset);
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
#ifdef DEBUG_PATCH
								printpatchfound("SwitcherPrs", NULL, OrigAddr);
#endif
								PatchBL(SwitcherPrsAddr, OrigAddr);
							}
						} break;
						case FCODE_DolEntryMod:
						{
							// HACK: PSO patch To Fake Entry
							u32 OrigAddr = FOffset + 0xAC;
							if ((read32(OrigAddr - 8) == 0x4C00012C) && (read32(OrigAddr) == 0x4E800021))  // isync and blrl
							{
#ifdef DEBUG_PATCH
								printpatchfound("PSO", "FakeEntry", OrigAddr);
#endif
								PatchBL(PATCH_OFFSET_ENTRY, OrigAddr);
							}
							else if (read32(FOffset + 0x38) == 0x4E800421) // bctrl
							{
								// HACK: Datel patch To Fake Entry
								PatchBL(PATCH_OFFSET_ENTRY, FOffset + 0x38);
								AppLoaderSize = read32(FOffset + 0x20);
								if ((AppLoaderSize & 0xFFFF0000) == 0x38800000)
									AppLoaderSize &= 0x0000FFFF;
								else
									AppLoaderSize = 0;
#ifdef DEBUG_PATCH
								printpatchfound("Datel", "FakeEntry", FOffset + 0x38);
								printpatchfound("Datel", "FakeEntrySize", AppLoaderSize);
#endif
							}
							else if (((u32)Buffer == 0x01300000) && (read32(FOffset + 0xA0) == 0x4C00012C) && (read32(FOffset + 0xC8) == 0x4E800021))  // isync and blrl
							{
#ifdef DEBUG_PATCH
								printpatchfound("Datel", "FakeEntry", FOffset + 0xC8);
#endif
								PatchBL(PATCH_OFFSET_ENTRY, FOffset + 0xC8);
							}
						} break;
						default:
						{
							if( CurPatterns[j].Patch == (u8*)ARQPostRequest )
							{
								if ((  (TITLE_ID) != 0x474D53  // Super Mario Sunshine
									&& (TITLE_ID) != 0x474C4D  // Luigis Mansion
									&& (TITLE_ID) != 0x475049  // Pikmin
									&& (TITLE_ID) != 0x474C56) // Chronicles of Narnia
									|| useipl == 1)
								{
									#ifdef DEBUG_PATCH
									dbgprintf("Patch:[ARQPostRequest] skipped (0x%08X)\r\n", FOffset);
									#endif
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
								#ifdef DEBUG_PATCH
								dbgprintf("Patch:Unhandled dead case:%08X\r\n", CurPatterns[j].Length );
								#endif
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
								#ifdef DEBUG_PATCH
								//dbgprintf("Setting [%s] to found!\r\n", CurPatterns[k].Name );
								#endif
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

	/* Check for PADInit, if not found use SIInit */
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
	free(hash);
	free(SHA1i);

	/*if( ((PatchCount & (1|2|4|8|2048)) != (1|2|4|8|2048)) )
	{
		#ifdef DEBUG_PATCH
		dbgprintf("Patch:Could not apply all required patches!\r\n");
		#endif
		Shutdown();
	}*/

	#ifdef DEBUG_PATCH
	//dbgprintf("PatchCount:%08X\r\n", PatchCount );
	//if( (PatchCount & FPATCH_DSP_ROM) == 0 )
	//	dbgprintf("Patch:Unknown DSP ROM\r\n");

	for(patitr = 0; patitr < PCODE_MAX; ++patitr)
	{
		if(AllFPatterns[patitr].patmode == PCODE_TRI && TRIGame == TRI_NONE)
			continue;
		if(AllFPatterns[patitr].patmode == PCODE_EXI && DisableEXIPatch)
			continue;
		if(AllFPatterns[patitr].patmode == PCODE_DATEL && Datel == 0)
			continue;
		if(AllFPatterns[patitr].patmode == PCODE_SI && DisableSIPatch)
			continue;
		FuncPattern *CurPatterns = AllFPatterns[patitr].pat;
		u32 CurPatternsLen = AllFPatterns[patitr].patlen;
		for( j=0; j < CurPatternsLen; ++j )
		{
			if(!CurPatterns[j].Found)
			{
				dbgprintf("Patch:[%s] not found\r\n", CurPatterns[j].Name );
				if(CurPatterns[j].Group)
					for( k = j; j < CurPatternsLen && CurPatterns[j].Group == CurPatterns[k].Group; ++j ) ;
			}
		}
	}
	#endif
	PatchState = PATCH_STATE_DONE;

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
	}*/
	if(useipl == 1)
	{
		//Force no audio resampling to avoid crashing
		if(read32(0x1341514) == 0x38600001)
		{
			write32(0x1341514, 0x38600000);
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched Gamecube NTSC IPL\r\n");
			#endif
		}
		else if(read32(0x136C9B4) == 0x38600001)
		{
			write32(0x136C9B4, 0x38600000);
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched Gamecube PAL IPL v1.0\r\n");
			#endif
		}
		else if(read32(0x1373618) == 0x38600001)
		{
			write32(0x1373618, 0x38600000);
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched Gamecube PAL IPL v1.2\r\n");
			#endif
		}
	}
	else if( TITLE_ID == 0x474256 )	// Batman Vengeance
	{
		// Skip Usage of EXI Channel 2
		if(write32A(0x0018B5DC, 0x60000000, 0x4801D6E1, 0))
		{
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched Batman Vengeance NTSC-U\r\n");
			#endif
		}
		else if(write32A(0x0015092C, 0x60000000, 0x4801D599, 0))
		{
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched Batman Vengeance PAL\r\n");
			#endif
		}
	}
	else if( TITLE_ID == 0x474336 )	// Pokemon Colosseum
	{
		// Memory Card inserted hack
		if( DisableEXIPatch == 0 )
		{
			if(write32A(0x000B0D88, 0x38600001, 0x4801C0B1, 0))
			{
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:Patched Pokemon Colosseum NTSC-J\r\n");
				#endif
			}
			else if(write32A(0x000B30DC, 0x38600001, 0x4801C2ED, 0))
			{
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:Patched Pokemon Colosseum NTSC-U\r\n");
				#endif
			}
			else if(write32A(0x000B66DC, 0x38600001, 0x4801C2ED, 0))
			{
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:Patched Pokemon Colosseum PAL\r\n");
				#endif
			}
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
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched WW NTSC-U\r\n");
			#endif
		}
		/* NTSC-U Demo */
		if(read32(0x0021D33C) == 0x40820034 && read32(0x00251EF8) == 0x41820068)
		{
			write32(0x0021D33C, 0x48000034);
			write32(0x00251EF8, 0x48000068);
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched WW NTSC-U Demo\r\n");
			#endif
		}
		/* PAL Final */
		if(read32(0x001F1FE0) == 0x40820034 && read32(0x0025B5C4) == 0x41820068)
		{
			write32(0x001F1FE0, 0x48000034);
			write32(0x0025B5C4, 0x48000068);
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched WW PAL\r\n");
			#endif
		}
		/* NTSC-J Final */
		if(read32(0x0021EDD4) == 0x40820034 && read32(0x00253BCC) == 0x41820068)
		{
			write32(0x0021EDD4, 0x48000034);
			write32(0x00253BCC, 0x48000068);
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched WW NTSC-J\r\n");
			#endif
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
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched X-Men Legends 2 NTSC-U\r\n");
			#endif
		}
	}
	PatchStaticTimers();

	sync_after_write( Buffer, Length );
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
	// Yea that & is needed, I'm not sure if the patch is needed at all
	// Commenting out until case where needed found
	//if ((GameEntry & 0x7FFFFFFF) < 0x31A0)
	//	Patch31A0();
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
	// Init Cache if its a new ISO
	if(TRIGame != TRI_SB)
		ISOSetupCache();
	// Reset SI status
	SIInit();
	u32 SiInitSet = 0;
	//Reset GCAM status
	GCAMInit();
	//F-Zero AX uses Clean CARD after 150 uses
	if(TRIGame == TRI_AX && TRI_BackupAvailable == 1)
	{
		sync_before_read(OurBase, 0x20);
		GCAMCARDCleanStatus(R16((u32)OurBase+0x16));
	}
	// Didn't look for why PMW2 requires this.  ToDo
	if ((TITLE_ID) == 0x475032 || TRIGame) // PacMan World 2 and Triforce hack
		SiInitSet = 1;
	write32(0x13002740, SiInitSet); //Clear SI Inited == 0
	write32(0x13002744, PADSwitchRequired() && (useipl == 0));
	sync_after_write((void*)0x13002740, 0x20);
	/* Clear main IRQ register */
	write32(EXI2CSR, 0);
	/* Clear AR positions */
	memset32((void*)0x131C0040, 0, 0x20);
	sync_after_write((void*)0x131C0040, 0x20);

	write32( FLUSH_LEN, FullLength >> 5 );
	u32 Command2 = DOLMinOff;
	// ToDo.  HACK: Why doesn't Nightfire Deep Descent level like this??
	if ((TITLE_ID) != 0x474f37)  // 007 Nightfire
		Command2 |= 0x80000000;
	write32( FLUSH_ADDR, Command2 );
	dbgprintf("Jumping to 0x%08X\n", GameEntry);
	sync_after_write((void*)0x1800, 0x1800); //low patches
	write32( RESET_STATUS, GameEntry );
	sync_after_write((void*)RESET_STATUS, 0x20);
	// in case we patched ipl remove status
	useipl = 0;
}

s32 Check_Cheats()
{
#ifdef CHEATS
	if((ConfigGetConfig(NIN_CFG_CHEATS)) ||
	  ((!(IsWiiU)) && ConfigGetConfig(NIN_CFG_DEBUGGER) ))
	{
		u32 DBGSize;
		FIL fs;
		
		if( f_open_char( &fs, "/sneek/kenobiwii.bin", FA_OPEN_EXISTING|FA_READ ) != FR_OK )
		{
			#ifdef DEBUG_PATCH
			dbgprintf( "Patch:Could not open:\"%s\", this file is required for debugging!\r\n", "/sneek/kenobiwii.bin" );
			#endif
			return -1;
		}
		DBGSize = fs.fsize;
		f_close( &fs );
		
		if( ConfigGetConfig( NIN_CFG_CHEATS ) )
		{
			char path[128];

			if( ConfigGetConfig(NIN_CFG_CHEAT_PATH) )
			{
				_sprintf( path, "%s", ConfigGetCheatPath() );
				if (path[0] == 0)	//cheat path is null
					return -4;
			} else {
				return 0;	//todo: temp until fixed
//				_sprintf( path, "/games/%.6s/%.6s.gct", (char*)0x1800, (char*)0x1800 );
//				_sprintf( path, "/games/%c%c%c%c%c%c/%c%c%c%c%c%c.gct", (char*)0, (char*)1, (char*)2, (char*)3, (char*)4, (char*)5, (char*)0, (char*)1, (char*)2, (char*)3, (char*)4, (char*)5 );
			}

			FIL CodeFD;

			if( f_open_char( &CodeFD, path, FA_OPEN_EXISTING|FA_READ ) == FR_OK )
			{
				if( CodeFD.fsize >= 0x2E60 - (0x1800+DBGSize-8) )
				{
					f_close( &CodeFD );
					#ifdef DEBUG_PATCH
					dbgprintf("Patch:Cheatfile is too large, it must not be large than %d bytes!\r\n", 0x2E60 - (0x1800+DBGSize-8));
					#endif
					return -3;
				}
				f_close( &CodeFD );
			}
			else
			{
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:Failed to read cheat file:\"%s\"\r\n", path );
				#endif
				return -2;
			}
		}
	}
#endif
	return 0;
}
