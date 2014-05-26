/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

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
#include "Config.h"
#include "global.h"
#include "patches.c"

//#define DEBUG_DSP  // Very slow!! Replace with raw dumps?

#define GAME_ID		(read32(0))
#define TITLE_ID	(GAME_ID >> 8)

u32 CardLowestOff = 0;
u32 POffset = 0x2F00;
vu32 Region = 0;

extern FIL GameFile;
extern u32 TRIGame;
extern u32 GameRegion;
extern bool SkipHandlerWait;

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
		0xDA, 0x39, 0xA3, 0xEE, 0x5E, 0x6B, 0x4B, 0x0D, 0x32, 0x55, 0xBF, 0xEF, 0x95, 0x60, 0x18, 0x90, 0xAF, 0xD8, 0x07, 0x09,			//	7
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
//	{
//		0x21, 0xD0, 0xC0, 0xEE, 0x25, 0x3D, 0x8C, 0x9E, 0x02, 0x58, 0x66, 0x7F, 0x3C, 0x1B, 0x11, 0xBC, 0x90, 0x1F, 0x33, 0xE2,			//	xx  --  Identical to 1
//	},
};

const unsigned char DSPPattern[][0x10] =
{
	{
		0x02, 0x9f, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00,		//	0 Hash 0, 5
	},
	{
		0x02, 0x9F, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00,		//	1
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9f, 0x0c, 0x10, 0x02, 0x9f, 0x0c, 0x1f, 0x02, 0x9f, 0x0c, 0x3b,		//	2 Hash 2
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9f, 0x0e, 0x76, 0x02, 0x9f, 0x0e, 0x85, 0x02, 0x9f, 0x0e, 0xa1,		//	3 Hash 3
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9f, 0x0e, 0xb3, 0x02, 0x9f, 0x0e, 0xc2, 0x02, 0x9f, 0x0e, 0xde,		//	4 Hash 4
	},
	{
		0x02, 0x9f, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00,		//	5 Hash 1
	},
	{
		0x02, 0x9f, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00,		//	6 Hash 8
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x71, 0x02, 0x9F, 0x0E, 0x80, 0x02, 0x9f, 0x0E, 0x9C,		//	7 Hash 9
	},
	{
		0x02, 0x9f, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00,		//	8 Hash 10
	},
	{
		0x02, 0x9F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00,		//	9 Hash 11
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x9F, 0x0E, 0x88, 0x02, 0x9F, 0x0E, 0x97, 0x02, 0x9F, 0x0E, 0xB3,		//	10 Hash 6
	},

};

const u32 DSPLength[] =
{
	0x00001D20,		//	0
	0x00001EC0,		//	1
	0x000019E0,		//	2
	0x00001EC0,		//	3
	0x00001F20,		//	4
	0x00001CE0,		//	5
	0x00001F00,		//	6
	0x00001EC0,		//	7
	0x00001A00,		//	8
	0x000017E0,		//	9
	0x00001F00,		//	10
};
void PatchAX_Dsp(u32 ptr, u32 Dup1, u32 Dup2, u32 Dup3, u32 Dup2Offset)
{
	static const u32 MoveLength = 0x20;
	static const u32 CopyLength = 0x12;
	static const u32 CallLength = 0x2 + 0x2; // call (2) ; jmp (2)
	static const u32 New1Length = 0x1 * 3 + 0x2 + 0x7; // 3 * orc (1) ; jmp (2) ; patch (7)
	static const u32 New2Length = 0x1; // ret
	u32 SourceAddr = Dup1 + MoveLength;
	u32 DestAddr = Dup2 + CallLength + CopyLength + New2Length;
	u32 Tmp;

	DestAddr--;
	W16((u32)ptr + (DestAddr)* 2, 0x02DF);  // nop (will be overwritten) ret
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
	W32((u32)ptr + (Dup1 + 0x02) * 2, 0x029F0000 | (Dup1 + MoveLength)); // jmp Dup1+0x16
	W32((u32)ptr + (Dup2 + 0x00) * 2, 0x02BF0000 | (Dup1 + CallLength)); // call Dup1+4
	W32((u32)ptr + (Dup2 + 0x02) * 2, 0x029F0000 | (Dup2 + MoveLength)); // jmp Dup2+0x16
	W32((u32)ptr + (Dup3 + 0x00) * 2, 0x02BF0000 | (Dup1 + CallLength)); // call Dup1+4
	W32((u32)ptr + (Dup3 + 0x02) * 2, 0x029F0000 | (Dup3 + MoveLength)); // jmp Dup3+0x16
	Tmp = R32((u32)ptr + (Dup1 + 0x98) * 2); // original instructions
	W32((u32)ptr + (Dup1 + 0x98) * 2, 0x02BF0000 | (Dup3 + CallLength)); // call Dup3+4
	W32((u32)ptr + (Dup2 + Dup2Offset) * 2, 0x02BF0000 | (Dup3 + CallLength)); // call Dup3+4

	W16((u32)ptr + (Dup3 + 0x04) * 2, Tmp >> 16); //  original instructions (Start of Dup3+4 [0xB long])
	W32((u32)ptr + (Dup3 + 0x05) * 2, 0x27D10340); // lrs         $AC1.M, @SampleFormat -
	W32((u32)ptr + (Dup3 + 0x07) * 2, 0x00038100); // andi        $AC1.M, #0x0003 clr         $ACC0
	W32((u32)ptr + (Dup3 + 0x09) * 2, 0x009E1FFF); // lri         $AC0.M, #0x1FFF
	W16((u32)ptr + (Dup3 + 0x0B) * 2, 0x02CA);     // lsrn
	W16((u32)ptr + (Dup3 + 0x0C) * 2, Tmp & 0xFFFF); //  original instructions
	W32((u32)ptr + (Dup3 + 0x0D) * 2, 0x3D0002DF); // andc        $AC1.M, $AC0.M ret

	W32((u32)ptr + (Dup3 + 0x0F) * 2, 0x27D10340); // lrs         $AC1.M, @SampleFormat -
	W32((u32)ptr + (Dup3 + 0x11) * 2, 0x00038100); // andi        $AC1.M, #0x0003 clr         $ACC0
	W32((u32)ptr + (Dup3 + 0x13) * 2, 0x009E1FFF); // lri         $AC0.M, #0x1FFF
	W16((u32)ptr + (Dup3 + 0x15) * 2, 0x02CA);     // lsrn
	Tmp = R32((u32)ptr + (Dup3 + 0x5D) * 2); // original instructions
	W16((u32)ptr + (Dup3 + 0x16) * 2, Tmp >> 16); //  original instructions
	W16((u32)ptr + (Dup3 + 0x17) * 2, 0x3D00); // andc        $AC1.M, $AC0.M
	W16((u32)ptr + (Dup3 + 0x18) * 2, Tmp & 0xFFFF); //  original instructions
	Tmp = R32((u32)ptr + (Dup3 + 0x5F) * 2); // original instructions
	W32((u32)ptr + (Dup3 + 0x19) * 2, Tmp); //  original instructions includes ret

	W32((u32)ptr + (Dup3 + 0x5D) * 2, 0x029F0000 | (Dup3 + CallLength + 0xB)); // jmp Dup3+0xF
	return;
}

bool write32A( u32 Offset, u32 Value, u32 CurrentValue, u32 ShowAssert )
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
	POffset -= sizeof(u32) * 5;
	
	u32 i;
	for (i = 0; i < (4 * 0x04); i+=0x04)
	{
		u32 Orig = *(vu32*)(0x319C + i);
		if ((Orig & 0xF4000002) == 0x40000000)
		{
			u32 NewAddr = (Orig & 0x3FFFFC) + 0x319C - POffset;
			Orig = (Orig & 0xFC000003) | NewAddr;
#ifdef DEBUG_PATCH
			dbgprintf("[%08X] Patch31A0 %08X: 0x%08X\r\n", 0x319C + i, POffset + i, Orig);
#endif
		}
		*(vu32*)(POffset + i) = Orig;
	}

	PatchB( POffset, 0x319C );
	PatchB( 0x31AC, POffset+0x10 );
}

void PatchFuncInterface( char *dst, u32 Length )
{
	int i;

	u32 LISReg=-1;
	u32 LISOff=-1;

	for( i=0; i < Length; i+=4 )
	{
		u32 op = read32( (u32)dst + i );

		if( (op & 0xFC1FFFFF) == 0x3C00CC00 )	// lis rX, 0xCC00
		{
			LISReg = (op & 0x3E00000) >> 21;
			LISOff = (u32)dst + i;
		}

		if( (op & 0xFC000000) == 0x38000000 )	// li rX, x
		{
			u32 src = (op >> 16) & 0x1F;
			u32 dst = (op >> 21) & 0x1F;
			if ((src != LISReg) && (dst == LISReg))
			{
				LISReg = -1;
				LISOff = (u32)dst + i;
			}
		}

		if( (op & 0xFC00FF00) == 0x38006C00 ) // addi rX, rY, 0x6C00 (ai)
		{
			u32 src = (op >> 16) & 0x1F;
			if( src == LISReg )
			{
				write32( (u32)LISOff, (LISReg<<21) | 0x3C00CD00 );	// Patch to: lis rX, 0xCD00
				#ifdef DEBUG_PATCH
				//dbgprintf("AI:[%08X] %08X: lis r%u, 0xCD00\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				#endif
				LISReg = -1;
			}
			u32 dst = (op >> 21) & 0x1F;
			if( dst == LISReg )
				LISReg = -1;
		}
		else if( (op & 0xFC00FF00) == 0x38006400 ) // addi rX, rY, 0x6400 (si)
		{
			u32 src = (op >> 16) & 0x1F;
			if( src == LISReg )
			{
				write32( (u32)LISOff, (LISReg<<21) | 0x3C00D302 );	// Patch to: lis rX, 0xD302
				//dbgprintf("SI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
				LISReg = -1;
			}
			u32 dst = (op >> 21) & 0x1F;
			if( dst == LISReg )
				LISReg = -1;
		}

		if( (op & 0xFC000000 ) == 0x80000000 )
		{
			u32 src = (op >> 16) & 0x1F;
			u32 dst = (op >> 21) & 0x1F;
			u32 val = op & 0xFFFF;
			
			if( src == LISReg )
			{
				if( (val & 0xFF00) == 0x6C00 ) // case with 0x6CXY(rZ) (ai)
				{
					write32( (u32)LISOff, (LISReg<<21) | 0x3C00CD00 );	// Patch to: lis rX, 0xCD00
					//dbgprintf("AI:[%08X] %08X: lis r%u, 0xCD00\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					LISReg = -1;
				}
				else if((val & 0xFF00) == 0x6400) // case with 0x64XY(rZ) (si)
				{
					write32( (u32)LISOff, (LISReg<<21) | 0x3C00D302 );	// Patch to: lis rX, 0xD302
					//dbgprintf("SI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					LISReg = -1;
				}
			}

			if( dst == LISReg )
				LISReg = -1;
		}

		if( (op & 0xFC000000 ) == 0x90000000 )
		{
			u32 src = (op >> 16) & 0x1F;
			u32 dst = (op >> 21) & 0x1F;
			u32 val = op & 0xFFFF;
			
			if( src == LISReg )
			{
				if( (val & 0xFF00) == 0x6C00 ) // case with 0x6CXY(rZ) (ai)
				{
					write32( (u32)LISOff, (LISReg<<21) | 0x3C00CD00 );	// Patch to: lis rX, 0xCD00
					//dbgprintf("AI:[%08X] %08X: lis r%u, 0xCD00\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					LISReg = -1;
				}
				else if((val & 0xFF00) == 0x6400) // case with 0x64XY(rZ) (si)
				{
					write32( (u32)LISOff, (LISReg<<21) | 0x3C00D302 );	// Patch to: lis rX, 0xD302
					//dbgprintf("SI:[%08X] %08X: lis r%u, 0xD302\r\n", (u32)LISOff, read32( (u32)LISOff), LISReg );
					LISReg = -1;
				}
			}

			if( dst == LISReg )
				LISReg = -1;
		}

		if( op == 0x4E800020 )	// blr
		{
			LISReg=-1;
		}
	}
}

void PatchFunc( char *ptr )
{
	u32 i	= 0;
	u32 reg=-1;

	while(1)
	{
		u32 op = read32( (u32)ptr + i );

		if( op == 0x4E800020 )	// blr
			break;

		if( (op & 0xFC00FFFF) == 0x3C00CC00 )	// lis rX, 0xCC00
		{
			reg = (op & 0x3E00000) >> 21;
			
			write32( (u32)ptr + i, (reg<<21) | 0x3C00C000 );	// Patch to: lis rX, 0xC000
			#ifdef DEBUG_PATCH
			dbgprintf("[%08X] %08X: lis r%u, 0xC000\r\n", (u32)ptr+i, read32( (u32)ptr+i), reg );
			#endif
		}
		
		if( (op & 0xFC00FFFF) == 0x3C00A800 )	// lis rX, 0xA800
		{			
			write32( (u32)ptr + i, (op & 0x3E00000) | 0x3C00A700 );		// Patch to: lis rX, 0xA700
			#ifdef DEBUG_PATCH
			dbgprintf("[%08X] %08X: lis rX, 0xA700\r\n", (u32)ptr+i, read32( (u32)ptr+i) );
			#endif
		}

		if( (op & 0xFC00FFFF) == 0x38006000 )	// addi rX, rY, 0x6000
		{
			u32 src = (op >> 16) & 0x1F;
			u32 dst = (op >> 21) & 0x1F;

			if( src == reg )
			{
				write32( (u32)ptr + i, (dst<<21) | (src<<16) | 0x38002F00 );	// Patch to: addi rX, rY, 0x2F00
				#ifdef DEBUG_PATCH
				dbgprintf("[%08X] %08X: addi r%u, r%u, 0x2F00\r\n", (u32)ptr+i, read32( (u32)ptr+i), dst, src );
				#endif

			}
		}

		if( (op & 0xFC000000 ) == 0x90000000 )
		{
			u32 src = (op >> 16) & 0x1F;
			u32 dst = (op >> 21) & 0x1F;
			u32 val = op & 0xFFFF;
			
			if( src == reg )
			{
				if( (val & 0xFF00) == 0x6000 )	// case with 0x60XY(rZ)
				{
					write32( (u32)ptr + i,  (dst<<21) | (src<<16) | 0x2F00 | (val&0xFF) | 0x90000000 );	// Patch to: stw rX, 0x2FXY(rZ)
					#ifdef DEBUG_PATCH
					dbgprintf("[%08X] %08X: stw r%u, 0x%04X(r%u)\r\n", (u32)ptr+i, read32( (u32)ptr+i), dst, 0x2F00 | (val&0xFF), src );
					#endif
				}
			}			
		}

		i += 4;
	}
}
void MPattern( u8 *Data, u32 Length, FuncPattern *FunctionPattern )
{
	u32 i;

	memset32( FunctionPattern, 0, sizeof(FuncPattern) );

	for( i = 0; i < Length; i+=4 )
	{
		u32 word = *(u32*)(Data + i);
		
		if( (word & 0xFC000003) ==  0x48000001 )
			FunctionPattern->FCalls++;

		if( (word & 0xFC000003) ==  0x48000000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x40800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x41800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x40810000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x41820000 )
			FunctionPattern->Branch++;
		
		if( (word & 0xFC000000) ==  0x80000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) ==  0x38000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) ==  0x3C000000 )
			FunctionPattern->Loads++;

		if( (word & 0xFC000000) ==  0x90000000 )
			FunctionPattern->Stores++;
		if( (word & 0xFC000000) ==  0x94000000 )
			FunctionPattern->Stores++;

		if( (word & 0xFF000000) ==  0x7C000000 )
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
	//dbzprintf("New Checksum: %04X %04X\r\n", *c1, *c2 );
} 
void DoDSPPatch( char *ptr, u32 Version )
{
	u32 Tmp;
	switch (Version)
	{
		case 0: // Zelda:WW
		{
			// 5F0 - before patch, part of halt routine
			W32((u32)ptr + (0x05F0 + 0) * 2, 0x02001000 ); // addi $AC0.M, 0x1000
			Tmp = R32((u32)ptr + (0x05B3 + 0) * 2); // original instructions at 0x5B3
			W32((u32)ptr + (0x05F0 + 2) * 2, Tmp); // original instructions at 0x5B3
			W32((u32)ptr + (0x05F0 + 4) * 2, 0x02DF02DF ); // ret/ret
		
			// 5F8 - before patch, part of halt routine
			W32((u32)ptr + (0x05F8 + 0) * 2, 0x02001000 ); // addi $AC0.M, 0x1000
			Tmp = R32((u32)ptr + (0x056C + 0) * 2); // original instructions at 0x56C
			W32((u32)ptr + (0x05F8 + 2) * 2, Tmp); // original instructions at 0x56C
			W32( (u32)ptr + (0x05F8 + 4) * 2, 0x02DF02DF ); // ret/ret

			// 5B3
			W32((u32)ptr + (0x05B3 + 0) * 2, 0x02BF05F0);  // call 0x05F0

			// 56C
			W32((u32)ptr + (0x056C + 0) * 2, 0x02BF05F8); // call 0x05F8
		} break;
		case 1:	// Mario Sunshine
		{
			// 0x5D0
			W32( (u32)ptr + 0xBA0 + 0, 0x02001000 );
			W32( (u32)ptr + 0xBA0 + 4, 0x1f7f1f5e );
			W32( (u32)ptr + 0xBA0 + 8, 0x02DF02DF );
						
			// 0x58E
			W32( (u32)ptr + 0xAF6 + 0, 0x02BF05D0 );

		} break;
		case 2:		// SSBM
		{
			PatchAX_Dsp( (u32)ptr, 0x5A8, 0x65D, 0x707, 0x8F);
		} break;
		case 3:		// Mario Party 5
		{
			PatchAX_Dsp( (u32)ptr, 0x6A3, 0x758, 0x802, 0x8F);
		} break;
		case 4:		// Beyond Good and Evil
		{
			PatchAX_Dsp( (u32)ptr, 0x6E0, 0x795, 0x83F, 0x8F);
		} break;
		case 5:
		{		
			// 5B2
			W32( (u32)ptr + 0xB64 + 0, 0x029F05F0 );

			// 5F0
			W32( (u32)ptr + 0xBE0 + 0, 0x02601000 );
			W32( (u32)ptr + 0xBE0 + 4, 0x02CA00FE );
			W32( (u32)ptr + 0xBE0 + 8, 0xFFD800FC );
			W32( (u32)ptr + 0xBE0 +12, 0xFFD902DF );

		} break;
		case 6:		// Star Fox Assault
		{
			PatchAX_Dsp( (u32)ptr, 0x69E, 0x753, 0x814, 0xA4);
		} break;
		case 7:		// ??? Need example
		{
		} break;
		case 8:			//	Donkey Kong Jungle Beat
		{
			// 6DD - before patch, part of halt routine
			W32((u32)ptr + (0x06A0 + 0x3D) * 2, 0x02001000 ); // addi $AC0.M, 0x1000
			Tmp = R32((u32)ptr + (0x06A0 + 0) * 2); // original instructions at 0x6A0
			W32((u32)ptr + (0x06A0 + 0x3F) * 2, Tmp); // original instructions at 0x6A0
			W32((u32)ptr + (0x06A0 + 0x41) * 2, 0x02DF02DF ); // ret/ret
		
			// 6E5 - before patch, part of halt routine
			W32((u32)ptr + (0x0659 + 0x8C) * 2, 0x02001000 ); // addi $AC0.M, 0x1000
			Tmp = R32((u32)ptr + (0x0659 + 0) * 2); // original instructions at 0x659
			W32((u32)ptr + (0x0659 + 0x8E) * 2, Tmp); // original instructions at 0x659
			W32( (u32)ptr + (0x0659 + 0x90) * 2, 0x02DF02DF ); // ret/ret

			// 6A0
			W32((u32)ptr + (0x06A0 + 0) * 2, 0x02BF06DD); // call 0x06DD

			// 659
			W32((u32)ptr + (0x0659 + 0) * 2, 0x02BF06E5); // call 0x06E5
		} break; 
		case 9:		// Paper Mario The Thousand Year Door
		{
			PatchAX_Dsp( (u32)ptr, 0x69E, 0x753, 0x7FD, 0x8F);
		} break;
		case 10:		// Animal Crossing
		{
			// CF4 - unused
			W32((u32)ptr + (0x0CF4 + 0) * 2, 0x02601000); // ori $AC0.M, 0x1000
			Tmp = R32((u32)ptr + (0x00C0 + 0) * 2); // original instructions at 0x0C0/0x10B
			W32((u32)ptr + (0x0CF4 + 2) * 2, Tmp); // original instructions at 0x0C0/0x10B
			W32((u32)ptr + (0x0CF4 + 4) * 2, 0x02DF0000); // ret/nop

			// 0C0
			W32((u32)ptr + (0x00C0 + 0) * 2, 0x02BF0CF4); // call 0x0CF4

			// 10B
			W32((u32)ptr + (0x010B + 0) * 2, 0x02BF0CF4); // call 0x0CF4
		} break;
		case 11:		// Luigi
		{
			// BE8 - unused
			W32((u32)ptr + (0x0BE8 + 0) * 2, 0x02601000); // ori $AC0.M, 0x1000
			Tmp = R32((u32)ptr + (0x00BE + 0) * 2); // original instructions at 0x0BE/0x109
			W32((u32)ptr + (0x0BE8 + 2) * 2, Tmp); // original instructions at 0x0BE/0x109
			W32((u32)ptr + (0x0BE8 + 4) * 2, 0x02DF0000); // ret/nop

			// 0BE
			W32((u32)ptr + (0x00BE + 0) * 2, 0x02BF0BE8); // call 0x0BE8

			// 109
			W32((u32)ptr + (0x0109 + 0) * 2, 0x02BF0BE8); // call 0x0BE8

		} break;
		default:
		{
		} break;
	}
}

#define PATCH_STATE_NONE  0
#define PATCH_STATE_LOAD  1
#define PATCH_STATE_PATCH 2
#define PATCH_STATE_DONE  4

u32 PatchState = PATCH_STATE_NONE;
u32 PSOHack    = 0;
u32 DOLSize	   = 0;
u32 DOLReadSize= 0;
u32 DOLMinOff  = 0;
u32 DOLMaxOff  = 0;
u32 DOLOffset  = 0;

void DoPatches( char *Buffer, u32 Length, u32 Offset )
{
	int i, j, k;
	u32 read;
	u32 value = 0;
	
	if( (u32)Buffer >= 0x01800000 )
		return;

	for( i=0; i < Length; i+=4 )
	{
		*(vu32*)(Buffer+i) = *(vu32*)(Buffer+i);
	}

	if( ((u32)Buffer <= 0x31A0)  && ((u32)Buffer + Length >= 0x31A0) )
	{
		#ifdef DEBUG_PATCH
		//dbgprintf("Patch:[Patch31A0]\r\n");
		#endif
		Patch31A0();
	}

	// PSO 1&2
	if( (TITLE_ID) == 0x47504F )
	{
		switch( Offset )
		{
			case 0x56B8E7E0:	// AppSwitcher	[EUR]
			case 0x56C49600:	// [USA] v1.1
			case 0x56C4C980:	// [USA] v1.0
			{
				PatchState	= PATCH_STATE_PATCH;
				DOLSize		= Length;
				DOLOffset	= (u32)Buffer;
				#ifdef DEBUG_PATCH
				dbgprintf("Patch: PSO 1&2 loading AppSwitcher:0x%p %u\r\n", Buffer, Length );
				#endif

			} break;
			case 0x5668FE20:	// psov3.dol [EUR]
			case 0x56750660:	// [USA] v1.1
			case 0x56753EC0:	// [USA] v1.0
			{
				#ifdef DEBUG_PATCH
				dbgprintf("Patch: PSO 1&2 loading psov3.dol:0x%p %u\r\n", Buffer, Length );
				#endif

				PSOHack = 1;
			} break;
		}
	}

	if( (PatchState & 0x3) == PATCH_STATE_NONE )
	{
		if( Length == 0x100 || PSOHack )
		{
			if( read32( (u32)Buffer ) == 0x100 )
			{
				//quickly calc the size
				DOLSize = sizeof(dolhdr);
				dolhdr *dol = (dolhdr*)Buffer;

				for( i=0; i < 7; ++i )
					DOLSize += dol->sizeText[i];
				for( i=0; i < 11; ++i )
					DOLSize += dol->sizeData[i];
						
				DOLReadSize = Length;

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

				if( PSOHack )
				{
					DOLMinOff = (u32)Buffer;
					DOLMaxOff = (u32)Buffer + DOLSize;
				}
#ifdef DEBUG_DI
				dbgprintf("DIP:DOLSize:%d DOLMinOff:0x%08X DOLMaxOff:0x%08X\r\n", DOLSize, DOLMinOff, DOLMaxOff );
#endif

				PatchState |= PATCH_STATE_LOAD;
			}
						
			PSOHack = 0;
		} else if( read32( (u32)Buffer ) == 0x7F454C46 )
		{
#ifdef DEBUG_DI
			dbgprintf("DIP:Game is loading an ELF 0x%08x (%u)\r\n", Offset, Length );
#endif
			DOLMinOff = (u32)Buffer;
			DOLOffset = Offset;
			DOLSize	  = 0;

			if( Length > 0x1000 )
				DOLReadSize = Length;
			else
				DOLReadSize = 0;

			Elf32_Ehdr *ehdr = (Elf32_Ehdr*)Buffer;
#ifdef DEBUG_DI
			dbgprintf("DIP:ELF Programheader Entries:%u\r\n", ehdr->e_phnum );	
#endif
			for( i=0; i < ehdr->e_phnum; ++i )
			{
				Elf32_Phdr phdr;
									
				f_lseek( &GameFile, DOLOffset + ehdr->e_phoff + i * sizeof(Elf32_Phdr) );
				f_read( &GameFile, &phdr, sizeof(Elf32_Phdr), &read );				

				DOLSize += (phdr.p_filesz+31) & (~31);	// align by 32byte
			}
			
#ifdef DEBUG_DI
			dbgprintf("DIP:ELF size:%u\r\n", DOLSize );
#endif

			PatchState |= PATCH_STATE_LOAD;
		}
	} 
	else if ( PatchState & PATCH_STATE_LOAD )
	{
		if( DOLReadSize == 0 )
			DOLMinOff = (u32)Buffer;

		DOLReadSize += Length;
		#ifdef DEBUG_PATCH
		dbgprintf("DIP:DOLsize:%d DOL read:%d\r\n", DOLSize, DOLReadSize );
		#endif
		if( DOLReadSize >= DOLSize )
		{
			DOLMaxOff = DOLMinOff + DOLSize;
			PatchState |= PATCH_STATE_PATCH;
			PatchState &= ~PATCH_STATE_LOAD;
		}
		if ((PatchState & PATCH_STATE_DONE) && (DOLMaxOff == DOLMinOff))
			PatchState = PATCH_STATE_DONE;
	}

	if (!(PatchState & PATCH_STATE_PATCH))
		return;
	
	sync_before_read( (char*)Buffer, Length );

	Buffer = (char*)DOLMinOff;
	Length = DOLMaxOff - DOLMinOff;
	#ifdef DEBUG_PATCH
	dbgprintf("Patch: Offset:0x%08X EOffset:0x%08X Length:%08X\r\n", DOLMinOff, DOLMaxOff, Length );

	dbgprintf("Patch:  Game ID = %x\r\n", read32(0));
	#endif

	// HACK: PokemonXD and Pokemon Colosseum low memory clear patch
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

	PatchFuncInterface( Buffer, Length );

	CardLowestOff = 0;
	
	u32 PatchCount = 64|128|1;
#ifdef CHEATS
	if( IsWiiU )
	{
		if( ConfigGetConfig(NIN_CFG_CHEATS) )
			PatchCount &= ~64;

	} else {

		if( ConfigGetConfig(NIN_CFG_DEBUGGER|NIN_CFG_CHEATS) )
			PatchCount &= ~64;
	}
#endif
	if( ConfigGetConfig(NIN_CFG_FORCE_PROG) || (ConfigGetVideoMode() & NIN_VID_FORCE) )
		PatchCount &= ~128;

	for( i=0; i < Length; i+=4 )
	{
		if( (PatchCount & 2048) == 0  )	// __OSDispatchInterrupt
		{
			if( read32((u32)Buffer + i + 0 ) == 0x3C60CC00 &&
				read32((u32)Buffer + i + 4 ) == 0x83E33000  
				)
			{
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:[__OSDispatchInterrupt] 0x%08X\r\n", (u32)Buffer + i );
				#endif
				POffset -= sizeof(FakeInterrupt);
				memcpy( (void*)(POffset), FakeInterrupt, sizeof(FakeInterrupt) );
				PatchBL( POffset, (u32)Buffer + i + 4 );
				
				// EXI Device 0 Control Register
				write32A( (u32)Buffer+i+0x114, 0x3C60C000, 0x3C60CC00, 1 );
				write32A( (u32)Buffer+i+0x118, 0x80830010, 0x80836800, 1 );

				PatchCount |= 2048;
			}
		}

		if( (PatchCount & 1) == 0 )	// 803463EC 8034639C
		if( (read32( (u32)Buffer + i )		& 0xFC00FFFF)	== 0x5400077A &&
			(read32( (u32)Buffer + i + 4 )	& 0xFC00FFFF)	== 0x28000000 &&
			 read32( (u32)Buffer + i + 8 )								== 0x41820008 &&
			(read32( (u32)Buffer + i +12 )	& 0xFC00FFFF)	== 0x64002000
			)  
		{
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Found [__OSDispatchInterrupt]: 0x%08X 0x%08X\r\n", (u32)Buffer + i + 0, (u32)Buffer + i + 0x1A8 );
			#endif
			write32( (u32)Buffer + i + 0x1A8,	(read32( (u32)Buffer + i + 0x1A8 )	& 0xFFFF0000) | 0x0463 );
			
			PatchCount |= 1;
		}

		if( (PatchCount & 2) == 0 )
		if( read32( (u32)Buffer + i )		== 0x5480056A &&
			read32( (u32)Buffer + i + 4 )	== 0x28000000 &&
			read32( (u32)Buffer + i + 8 )	== 0x40820008 &&
			(read32( (u32)Buffer + i +12 )&0xFC00FFFF) == 0x60000004
			) 
		{
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Found [SetInterruptMask]: 0x%08X\r\n", (u32)Buffer + i + 12 );
			#endif

			write32( (u32)Buffer + i + 12, (read32( (u32)Buffer + i + 12 ) & 0xFFFF0000) | 0x4004 );

			PatchCount |= 2;
		}

		if( (PatchCount & 4) == 0 )
		if( (read32( (u32)Buffer + i + 0 ) & 0xFFFF) == 0x6000 &&
			(read32( (u32)Buffer + i + 4 ) & 0xFFFF) == 0x002A &&
			(read32( (u32)Buffer + i + 8 ) & 0xFFFF) == 0x0054 
			) 
		{
			u32 Offset = (u32)Buffer + i - 8;
			u32 HwOffset = Offset;
			if ((read32(Offset + 4) & 0xFFFF) == 0xCC00)	// Loader
				HwOffset = Offset + 4;
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Found [__DVDIntrruptHandler]: 0x%08X (0x%08X)\r\n", Offset, HwOffset );
			#endif
			POffset -= sizeof(DCInvalidateRange);
			memcpy( (void*)(POffset), DCInvalidateRange, sizeof(DCInvalidateRange) );
			PatchBL(POffset, (u32)HwOffset);
							
			Offset += 92;
						
			if( (read32(Offset-8) & 0xFFFF) == 0xCC00 )	// Loader
			{
				Offset -= 8;
			}
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:[__DVDInterruptHandler] 0x%08X\r\n", Offset );	
			#endif
			
			value = *(vu32*)Offset;
			value&= 0xFFFF0000;
			value|= 0x0000CD80;
			*(vu32*)Offset = value;

			PatchCount |= 4;
		}
		
		if( (PatchCount & 8) == 0 )
		{
			if( (read32( (u32)Buffer + i + 0 ) & 0xFFFF) == 0xCC00 &&			// Game
				(read32( (u32)Buffer + i + 4 ) & 0xFFFF) == 0x6000 &&
				(read32( (u32)Buffer + i +12 ) & 0xFFFF) == 0x001C 
				) 
			{
				u32 Offset = (u32)Buffer + i;
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset );
				#endif
				value = *(vu32*)(Offset+8);
				value&= 0x03E00000;
				value|= 0x38000000;
				*(vu32*)(Offset+8) = value;
				
				u32 SearchIndex = 0;
				for (SearchIndex = 0; SearchIndex < 2; SearchIndex++)
				{
					if (write32A(Offset + 0x1CC, 0x3C60C000, 0x3C60CC00, 0))
						dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset + 0x1CC);
					if (write32A(Offset + 0x1D0, 0x80032f50, 0x80036020, 0))
						dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset + 0x1D0);

					if (write32A(Offset + 0x1DC, 0x3C60C000, 0x3C60CC00, 0))
						dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset + 0x1DC);
					if (write32A(Offset + 0x1E0, 0x38632f30, 0x38636000, 0))
						dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset + 0x1E0);

					if (write32A(Offset + 0x238, 0x3C60c000, 0x3C60CC00, 0))
						dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset + 0x238);
					if (write32A(Offset + 0x23C, 0x80032f50, 0x80036020, 0))
						dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset + 0x23C);
					Offset += 0x4C;
				}

				PatchCount |= 8;

			} else if(	(read32( (u32)Buffer + i + 0 ) & 0xFFFF) == 0xCC00 && // Loader
						(read32( (u32)Buffer + i + 4 ) & 0xFFFF) == 0x6018 &&
						(read32( (u32)Buffer + i +12 ) & 0xFFFF) == 0x001C 
				)
			{
				u32 Offset = (u32)Buffer + i;
				#ifdef DEBUG_PATCH			
				dbgprintf("Patch:[cbForStateBusy] 0x%08X\r\n", Offset );
				#endif
				write32( Offset, 0x3C60C000 );
				write32( Offset+4, 0x80832F48 );
		
				PatchCount |= 8;
				
			}
		}

		if( (PatchCount & 16) == 0 )
		{
			if( read32( (u32)Buffer + i + 0 ) == 0x3C608000 )
			{
				if( ((read32( (u32)Buffer + i + 4 ) & 0xFC1FFFFF ) == 0x800300CC) && ((read32( (u32)Buffer + i + 8 ) >> 24) == 0x54 ) )
				{
					#ifdef DEBUG_PATCH
					dbgprintf( "Patch:[VIConfgiure] 0x%08X\r\n", (u32)(Buffer+i) );
					#endif
					write32( (u32)Buffer + i + 4, 0x5400F0BE | ((read32( (u32)Buffer + i + 4 ) & 0x3E00000) >> 5	) );
					PatchCount |= 16;
				}
			}
		}

		if( (PatchCount & 32) == 0 )
		{
			if( read32( (u32)Buffer + i + 0 ) == 0xA063501A )
			{
				if( read32( (u32)Buffer + i + 0x10 ) == 0x80010014  )
				{
					write32( (u32)Buffer + i + 0x10, 0x3800009C );
					#ifdef DEBUG_PATCH
					dbgprintf( "Patch:[ARInit] 0x%08X\r\n", (u32)(Buffer+i) );
					#endif
				}
//				else
//				{
					#ifdef DEBUG_PATCH
//					dbgprintf("Patch skipped:[ARInit] 0x%08X\r\n", (u32)(Buffer + i));
					#endif
//				}

				PatchCount |= 32;
			}
		}
#ifdef CHEATS
		if( (PatchCount & 64) == 0 )
		{
			//OSSleepThread(Pattern 1)
			if( read32((u32)Buffer + i + 0 ) == 0x3C808000 &&
				( read32((u32)Buffer + i + 4 ) == 0x3C808000 || read32((u32)Buffer + i + 4 ) == 0x808400E4 ) &&
				( read32((u32)Buffer + i + 8 ) == 0x38000004 || read32((u32)Buffer + i + 8 ) == 0x808400E4 )
			)
			{
				int j = 12;

				while( *(vu32*)(Buffer+i+j) != 0x4E800020 )
					j+=4;


				u32 DebuggerHook = (u32)Buffer + i + j;
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:[Hook:OSSleepThread] at 0x%08X\r\n", DebuggerHook | 0x80000000 );
				#endif

				u32 DBGSize;

				FIL fs;
				if( f_open( &fs, "/sneek/kenobiwii.bin", FA_OPEN_EXISTING|FA_READ ) != FR_OK )
				{
					#ifdef DEBUG_PATCH
					dbgprintf( "Patch:Could not open:\"%s\", this file is required for debugging!\r\n", "/sneek/kenobiwii.bin" );
					#endif

				} else {
										
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

						} else {

							f_close( &fs );

							if( IsWiiU )
							{
								*(vu32*)(P2C(*(vu32*)0x1808)) = 0;

							} else {

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

								if( f_open( &CodeFD, path, FA_OPEN_EXISTING|FA_READ ) == FR_OK )
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

				PatchCount |= 64;
			}
		}
#endif
		if( (PatchCount & 128) == 0  )
		{
			u32 k=0;
			for( j=0; j <= GXNtsc480Int; ++j )	// Don't replace the Prog struct
			{
				if( memcmp( Buffer+i, GXMObjects[j], 0x3C ) == 0 )
				{
					#ifdef DEBUG_PATCH
					dbgprintf("Patch:Found GX pattern %u at %08X\r\n", j, (u32)Buffer+i );
					#endif
					if( ConfigGetConfig(NIN_CFG_FORCE_PROG) )
					{
						switch( ConfigGetVideoMode() & NIN_VID_FORCE_MASK )
						{
							case NIN_VID_FORCE_NTSC:
							{
								memcpy( Buffer+i, GXMObjects[GXNtsc480Prog], 0x3C );
							} break;
							case NIN_VID_FORCE_PAL50:
							{
								memcpy( Buffer+i, GXMObjects[GXPal528Prog], 0x3C );
							} break;
							case NIN_VID_FORCE_PAL60:
							{
								memcpy( Buffer+i, GXMObjects[GXEurgb60Hz480Prog], 0x3C );
							} break;
						}

					} else {

						if( ConfigGetVideoMode() & NIN_VID_FORCE )
						{
							switch( ConfigGetVideoMode() & NIN_VID_FORCE_MASK )
							{
								case NIN_VID_FORCE_NTSC:
								{
									memcpy( Buffer+i, GXMObjects[GXNtsc480IntDf], 0x3C );
								} break;
								case NIN_VID_FORCE_PAL50:
								{
									memcpy( Buffer+i, GXMObjects[GXPal528IntDf], 0x3C );
								} break;
								case NIN_VID_FORCE_PAL60:
								{
									memcpy( Buffer+i, GXMObjects[GXEurgb60Hz480IntDf], 0x3C );
								} break;
								case NIN_VID_FORCE_MPAL:
								{
									memcpy( Buffer+i, GXMObjects[GXMpal480IntDf], 0x3C );
								} break;
							}
						
						} else {

							switch( Region )
							{
								default:
								case 1:
								case 0:
								{
									memcpy( Buffer+i, GXMObjects[GXNtsc480IntDf], 0x3C );
								} break;
								case 2:
								{
									memcpy( Buffer+i, GXMObjects[GXEurgb60Hz480IntDf], 0x3C );
								} break;
							}
						}
					}
					k++;
				}
			}

			if( k > 4 )
			{
				PatchCount |= 128;
			}	
		}

		if( (PatchCount & 256) == 0 )	//DVDLowStopMotor
		{
			if( read32( (u32)Buffer + i ) == 0x3C00E300 )
			{
				u32 Offset = (u32)Buffer + i;
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:[DVDLowStopMotor] 0x%08X\r\n", Offset );
				#endif
				
				value = *(vu32*)(Offset-12);
				value&= 0xFFFF0000;
				value|= 0x0000C000;
				*(vu32*)(Offset-12) = value;

				value = *(vu32*)(Offset-8);
				value&= 0xFFFF0000;
				value|= 0x00002F00;
				*(vu32*)(Offset-8) = value;

				value = *(vu32*)(Offset+4);
				value&= 0xFFFF0000;
				value|= 0x00002F08;
				*(vu32*)(Offset+4) = value;		

				PatchCount |= 256;
			}
		}

		if( (PatchCount & 512) == 0 )	//DVDLowReadDiskID
		{
			if( (read32( (u32)Buffer + i ) & 0xFC00FFFF ) == 0x3C00A800 && (read32( (u32)Buffer + i + 4 ) & 0xFC00FFFF ) == 0x38000040 )
			{
				u32 Offset = (u32)Buffer + i;
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:[DVDLowReadDiskID] 0x%08X\r\n", Offset );
				#endif

				value = *(vu32*)(Offset);
				value&= 0xFFFF0000;
				value|= 0x0000A700;
				*(vu32*)(Offset) = value;

				value = *(vu32*)(Offset+0x20);
				value&= 0xFFFF0000;
				value|= 0x0000C000;
				*(vu32*)(Offset+0x20) = value;
				
				value = *(vu32*)(Offset+0x24);
				value&= 0xFFFF0000;
				value|= 0x00002F00;
				*(vu32*)(Offset+0x24) = value;

				value = *(vu32*)(Offset+0x2C);
				value&= 0xFFFF0000;
				value|= 0x00002F08;
				*(vu32*)(Offset+0x2C) = value;

				PatchCount |= 512;				
			}
		}

		if( (PatchCount & 1024) == 0  )	// DSP patch
		{
			u32 l,m,Known=-1;

			// for each pattern/length combo
			for( l=0; l < sizeof(DSPPattern) / 0x10; ++l )
			{
				if( memcmp( (void*)(Buffer+i), DSPPattern[l], 0x10 ) == 0 )
				{
					#ifdef DEBUG_PATCH
					dbgprintf("Patch:[DSPPattern] 0x%08X v%u\r\n", (u32)Buffer + i, l );
					#endif
					u8 *SHA1i = (u8*)malloca( 0x60, 0x40 );
					u8 *hash  = (u8*)malloca( 0x14, 0x40 );

					memcpy( (void*)0x11200000, (unsigned char*)Buffer+i, DSPLength[l] );
					
					sha1( SHA1i, NULL, 0, 0, NULL );
					sha1( SHA1i, (void*)0x11200000, DSPLength[l], 2, hash );

					// for each hash - doesn't have to match Pattern index (l)
					for( m=0; m < sizeof(DSPHashes) / 0x14; ++m )
					{
						if( memcmp( DSPHashes[m], hash, 0x14 ) == 0 )
						{
							Known = m;
#ifdef DEBUG_DSP
							dbgprintf("DSP before Patch\r\n");
							hexdump((void*)(Buffer + i), DSPLength[l]);
#endif
							DoDSPPatch(Buffer + i, m);
#ifdef DEBUG_DSP
							dbgprintf("DSP after Patch\r\n");
							hexdump((void*)(Buffer + i), DSPLength[l]);
#endif
							break;
						}
					}

					if( Known != -1 )
					{
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[DSPROM] DSPv%u\r\n", Known );
						#endif
						PatchCount |= 1024;
						break;
					}

					free(hash);
					free(SHA1i);
				}
			}
		}

		if( PatchCount == (1|2|4|8|16|32|64|128|256|512|1024|2048) )
			break;
	}
	#ifdef DEBUG_PATCH
	dbgprintf("PatchCount:%08X\r\n", PatchCount );

	if( (PatchCount & 1024) == 0  )	
	{
		dbgprintf("Patch:Unknown DSP ROM\r\n");
	}
	#endif

	if( (PatchCount & (1|2|4|8|2048)) != (1|2|4|8|2048) )
	{
		#ifdef DEBUG_PATCH
		dbgprintf("Patch:Could not apply all required patches!\r\n");
		#endif
		Shutdown();
	}
	
	for( i=0; i < Length; i+=4 )
	{
		if( *(u32*)(Buffer + i) != 0x4E800020 )
			continue;

		i+=4;

		FuncPattern fp;
		MPattern( (u8*)(Buffer+i), Length, &fp );

		for( j=0; j < sizeof(FPatterns)/sizeof(FuncPattern); ++j )
		{
			if( FPatterns[j].Found ) //Skip already found patches
				continue;
							
			if( CPattern( &fp, &(FPatterns[j]) ) )
			{
				u32 FOffset = (u32)Buffer + i;

				FPatterns[j].Found = FOffset;
				#ifdef DEBUG_PATCH
				dbgprintf("Patch:Found [%s]: 0x%08X\r\n", FPatterns[j].Name, FOffset );
				#endif

				switch( FPatterns[j].PatchLength )
				{
					case 0xdead0002:	// DVDLowRead
					{
						PatchFunc( (char*)FOffset );
					} break;
					case 0xdead0003:	// __AI_set_stream_sample_rate
					{
						write32( FOffset, 0x4E800020 );
					} break;
					case 0xdead0004:	// Audiostreaming hack
					{
						switch( TITLE_ID )
						{
							case 0x474544:	// Eternal Darkness
							break;
							default:
							{
								write32( FOffset + 0xB4, 0x60000000 );
								write32( FOffset + 0xC0, 0x60000000 );
							} break;
						}
					} break;
					case 0xdead0005:	// GXInitTlutObj
					{
						if( read32( FOffset+0x34 ) == 0x5400023E )
						{
							#ifdef DEBUG_PATCH
							dbgprintf("Patch:[GXInitTlutObj] 0x%08X\r\n", FOffset );
							#endif
							write32( FOffset+0x34, 0x5400033E );

						} else if( read32( FOffset+0x28 ) == 0x5004C00E )
						{
							#ifdef DEBUG_PATCH
							dbgprintf("Patch:[GXInitTlutObj] 0x%08X\r\n", FOffset );
							#endif
							write32( FOffset+0x28, 0x5004C016 );
						} else {
							FPatterns[j].Found = 0; // False hit
						}
					} break;
					case 0xdead0008:	// __ARChecksize
					{
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[__ARChecksize] 0x%08X\r\n", FOffset );
						#endif
						u32 EndOffset = FOffset + FPatterns[j].Length;
						u32 Register = (*(vu32*)(EndOffset-24) >> 21) & 0x1F;

						*(vu32*)(FOffset)	= 0x3C000100 | ( Register << 21 );
						*(vu32*)(FOffset+4)	= *(vu32*)(EndOffset-28);
						*(vu32*)(FOffset+8) = *(vu32*)(EndOffset-24);
						*(vu32*)(FOffset+12)= *(vu32*)(EndOffset-20);
						*(vu32*)(FOffset+16)= 0x4E800020;

					} break;
// Widescreen hack by Extrems
					case 0xdead000C:	//	C_MTXPerspective
					{
						if( !ConfigGetConfig(NIN_CFG_FORCE_WIDE) )
							break;
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[C_MTXPerspective] 0x%08X\r\n", FOffset );
						#endif
						*(volatile float *)0x50 = 0.5625f;

						memcpy((void*)(FOffset+ 28),(void*)(FOffset+ 36),44);
						memcpy((void*)(FOffset+188),(void*)(FOffset+192),16);

						*(unsigned int*)(FOffset+52) = 0x48000001 | ((*(unsigned int*)(FOffset+52) & 0x3FFFFFC) + 8);
						*(unsigned int*)(FOffset+72) = 0x3C600000 | (0x80000050 >> 16);		// lis		3, 0x8180
						*(unsigned int*)(FOffset+76) = 0xC0230000 | (0x80000050 & 0xFFFF);	// lfs		1, -0x1C (3)
						*(unsigned int*)(FOffset+80) = 0xEC240072;							// fmuls	1, 4, 1

					} break;
					case 0xdead000D:	//	C_MTXLightPerspective
					{
						if( !ConfigGetConfig(NIN_CFG_FORCE_WIDE) )
							break;
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[C_MTXLightPerspective] 0x%08X\r\n", FOffset );
						#endif
						*(volatile float *)0x50 = 0.5625f;

						*(u32*)(FOffset+36) = *(u32*)(FOffset+32);

						memcpy((void*)(FOffset+ 28),(void*)(FOffset+ 36),60);
						memcpy((void*)(FOffset+184),(void*)(FOffset+188),16);

						*(u32*)(FOffset+68) += 8;
						*(u32*)(FOffset+88) = 0x3C600000 | (0x80000050 >> 16); 		// lis		3, 0x8180
						*(u32*)(FOffset+92) = 0xC0230000 | (0x80000050 & 0xFFFF);	// lfs		1, -0x90 (3)
						*(u32*)(FOffset+96) = 0xEC240072;							// fmuls	1, 4, 1

					} break;
					case 0xdead0010:	//	clip
					{
						if( !ConfigGetConfig(NIN_CFG_FORCE_WIDE) )
							break;
						
						write32( FOffset,   0x38600000 );
						write32( FOffset+4, 0x4E800020 );
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[Clip] 0x%08X\r\n", FOffset );
						#endif
					} break;
					case 0xdead000E:	//	__OSReadROM
					{
						memcpy( (void*)FOffset, __OSReadROM, sizeof(__OSReadROM) );
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:[__OSReadROM] 0x%08X\r\n", FOffset );
						#endif
					} break;
					case 0xdead000F:	// Patch for __GXSetVAT, fixes the dungeon map freeze in Wind Waker
					{
						switch( TITLE_ID )
						{
							case 0x505A4C:	// The Legend of Zelda: Collector's Edition
								if( !(DOLSize == 3847012 || DOLSize == 3803812) )			// only patch the main.dol of the Zelda:ww game 
									break;
							case 0x475A4C:	// The Legend of Zelda: The Wind Waker
							{
								write32(FOffset, (read32(FOffset) & 0xff00ffff) | 0x00220000);
								memcpy( (void *)(FOffset + 4), __GXSetVAT_patch, sizeof(__GXSetVAT_patch) );	
								#ifdef DEBUG_PATCH
								dbgprintf("Patch:Applied __GXSetVAT patch\r\n");
								#endif
							} break;
							default:
							break;
						}			
					} break;
					case 0xdead0020:
					{
						u32 PatchOffset = 0x4;
						while ((read32(FOffset + PatchOffset) != 0x90010004) && (PatchOffset < 0x20)) // MaxSearch=0x20
							PatchOffset += 4;
						if (read32(FOffset + PatchOffset) != 0x90010004) 	// stw r0, 0x0004(sp)
						{
							#ifdef DEBUG_PATCH
							dbgprintf("Patch:Skipped **IntrruptHandler patch 0x%X (PatchOffset=Not Found) \r\n", FOffset, PatchOffset);
							#endif
							break;
						}

						POffset -= sizeof(TCIntrruptHandler);
						memcpy((void*)(POffset), TCIntrruptHandler, sizeof(TCIntrruptHandler));

						write32(POffset, read32((FOffset + PatchOffset)));

						PatchBL( POffset, (FOffset + PatchOffset) );						
						#ifdef DEBUG_PATCH
						dbgprintf("Patch:Applied **IntrruptHandler patch 0x%X (PatchOffset=0x%X) \r\n", FOffset, PatchOffset);
						#endif
					} break;
					case 0xdead0021:	// EXIDMA
					{
						u32 off		= 0;
						u32 reg		=-1;
						u32 valueB	= 0;

						while(1)
						{
							off += 4;

							u32 op = read32( (u32)FOffset + off );
								
							if( reg == -1 )
							if( (op & 0xFC1F0000) == 0x3C000000 )	// lis rX, 0xCC00
							{
								reg = (op & 0x3E00000) >> 21;
								if( reg == 3 )
								{
									value  = read32( FOffset + off ) & 0x0000FFFF;
									#ifdef DEBUG_PATCH
									//dbgprintf("lis:%08X value:%04X\r\n", FOffset+off, value );
									#endif
								}
							}

							if( reg != -1 )
							if( (op & 0xFC000000) == 0x38000000 )	// addi rX, rY, z
							{
								u32 src = (op >> 16) & 0x1F;

								if( src == reg )
								{
									valueB = read32( FOffset + off ) & 0x0000FFFF;
									#ifdef DEBUG_PATCH
									//dbgprintf("addi:%08X value:%04X\r\n", FOffset+off, valueB);
									#endif
									break;
								}
							}

							if( op == 0x4E800020 )	// blr
								break;
						}
						
						if( valueB & 0x8000 )
							value =  (( value << 16) - (((~valueB) & 0xFFFF)+1) );
						else
							value =  (value << 16) + valueB;
						#ifdef DEBUG_PATCH
						//dbgprintf("F:%08X\r\n", value );
						#endif

						memcpy( (void*)(FOffset), EXIDMA, sizeof(EXIDMA) );
						
						valueB = 0x90E80000 | (value&0xFFFF);
						value  = 0x3D000000 | (value>>16);
						
						if( valueB & 0x8000 )
							value++;

						valueB+=4;
						
						write32( FOffset+0x10, value );
						write32( FOffset+0x14, valueB );

					} break;
					case 0xdead0022:	// CARD timeout
					{
						write32( FOffset+0x124, 0x60000000 );	
						write32( FOffset+0x18C, 0x60000000 );	

					} break;
					case 0xdead0023:	// CARD timeout
					{
						write32( FOffset+0x118, 0x60000000 );	
						write32( FOffset+0x180, 0x60000000 );	

					} break;
					default:
					{
						if( ConfigGetConfig( NIN_CFG_DEBUGGER ) || !ConfigGetConfig(NIN_CFG_OSREPORT) )
						{
							if( FPatterns[j].Patch == patch_fwrite_GC )
							{
								#ifdef DEBUG_PATCH
								dbgprintf("Patch:Skipped [patch_fwrite_GC]\r\n");
								#endif
								break;
							}
						}

						if( IsWiiU )
						{
							if( FPatterns[j].Patch == patch_fwrite_GC )
							{
								#ifdef DEBUG_PATCH
								dbgprintf("Patch:Skipped [patch_fwrite_GC]\r\n");
								#endif
								break;
							}
						}

						if( FPatterns[j].Patch == (u8*)ARQPostRequest )
						{
							if( (TITLE_ID) == 0x47414C ||	// Super Smash Bros Melee
								(TITLE_ID) == 0x474D38 ||	// Metroid Prime
								(TITLE_ID) == 0x47324D ||	// Metroid Prime 2
								(TITLE_ID) == 0x474B59 ||	// Kirby Air Ride
								(TITLE_ID) == 0x475852 ||	// Mega Man X Command Mission
								(TITLE_ID) == 0x474654)		// Mario Golf Toadstool Tour
							{
								#ifdef DEBUG_PATCH
								dbgprintf("Patch:Skipped [ARQPostRequest]\r\n");
								#endif
								break;
							}
						}
						if( FPatterns[j].Patch == (u8*)DVDGetDriveStatus )
						{
							if( (TITLE_ID) != 0x474754 &&	// Chibi-Robo!
								(TITLE_ID) != 0x475041 )	// Pok駑on Channel
								break;
							#ifdef DEBUG_PATCH
							dbgprintf("Patch:DVDGetDriveStatus\r\n");
							#endif
						}

						if( (FPatterns[j].Length >> 16) == 0xdead )
						{
							#ifdef DEBUG_PATCH
							dbgprintf("DIP:Unhandled dead case:%08X\r\n", FPatterns[j].Length );
							#endif
							
						} else
						{

							memcpy( (void*)(FOffset), FPatterns[j].Patch, FPatterns[j].PatchLength );
						}

					} break;
				}

				// If this is a patch group set all others of this group as found aswell
				if( FPatterns[j].Group )
				{
					for( k=0; k < sizeof(FPatterns)/sizeof(FuncPattern); ++k )
					{
						if( FPatterns[k].Group == FPatterns[j].Group )
						{
							if( !FPatterns[k].Found )		// Don't overwrite the offset!
								FPatterns[k].Found = -1;	// Usually this holds the offset, to determinate it from a REALLY found pattern we set it -1 which still counts a logical TRUE
							#ifdef DEBUG_PATCH
							//dbgprintf("Setting [%s] to found!\r\n", FPatterns[k].Name );
							#endif
						}
					}
				}
			}
		}
	}

	PatchState = PATCH_STATE_DONE;
		
	if( (GAME_ID & 0xFFFFFF00) == 0x475A4C00 )	// GZL=Wind Waker
	{
		//Anti FrameDrop Panic
		/* NTSC-U Final */
		if(*(vu32*)(0x00221A28) == 0x40820034 && *(vu32*)(0x00256424) == 0x41820068)
		{
			//	write32( 0x03945B0, 0x8039408C );	// Test menu
			*(vu32*)(0x00221A28) = 0x48000034;
			*(vu32*)(0x00256424) = 0x48000068;
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched WW NTSC-U\r\n");
			#endif
		}
		/* NTSC-U Demo */
		if(*(vu32*)(0x0021D33C) == 0x40820034 && *(vu32*)(0x00251EF8) == 0x41820068)
		{
			*(vu32*)(0x0021D33C) = 0x48000034;
			*(vu32*)(0x00251EF8) = 0x48000068;
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched WW NTSC-U Demo\r\n");
			#endif
		}
		/* PAL Final */
		if(*(vu32*)(0x001F1FE0) == 0x40820034 && *(vu32*)(0x0025B5C4) == 0x41820068)
		{
			*(vu32*)(0x001F1FE0) = 0x48000034;
			*(vu32*)(0x0025B5C4) = 0x48000068;
			#ifdef DEBUG_PATCH
			dbgprintf("Patch:Patched WW PAL\r\n");
			#endif
		}
		/* NTSC-J Final */
		if(*(vu32*)(0x0021EDD4) == 0x40820034 && *(vu32*)(0x00253BCC) == 0x41820068)
		{
			*(vu32*)(0x0021EDD4) = 0x48000034;
			*(vu32*)(0x00253BCC) = 0x48000068;
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
	/*else if( ((GAME_ID & 0xFFFFFF00) == 0x47414C00) || GAME_ID == 0x474C4D45 ) //Melee or NTSC Luigis Mansion
	{
		#ifdef DEBUG_PATCH
		dbgprintf("Patch:Skipping Wait for Handler\r\n");
		#endif
		SkipHandlerWait = true; //some patch doesnt get applied so without this it would freeze
	}*/

	for( j=0; j < sizeof(FPatterns)/sizeof(FuncPattern); ++j )
	{
		if( FPatterns[j].Found ) //Skip already found patches
			continue;

		#ifdef DEBUG_PATCH
		dbgprintf("Patch: [%s] not found\r\n", FPatterns[j].Name );
		#endif
	}

	return;
}
