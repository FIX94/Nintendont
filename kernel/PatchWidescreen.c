/*
PatchWidescreen.c for Nintendont (Kernel)

Copyright (C) 2014 FIX94

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
#include "debug.h"
#include "common.h"
#include "string.h"
#include "PatchWidescreen.h"
#include "asm/CalcWidescreen.h"
#include "asm/CalcWidescreenDiv.h"
#include "asm/CalcGXWidescreen.h"

extern void PatchB(u32 dst, u32 src);
extern u32 PatchCopy(const u8 *PatchPtr, const u32 PatchSize);
void PatchWideMulti(u32 BufferPos, u32 dstReg)
{
	u32 wide = PatchCopy(CalcWidescreen, CalcWidescreen_size);
	/* Copy original instruction and jump */
	write32(wide, read32(BufferPos));
	PatchB(wide, BufferPos);
	/* Modify destination register */
	write32(wide+0x28, (read32(wide+0x28) & 0xFC1FF83F) | (dstReg << 6) | (dstReg << 21));
	/* Jump back to original code */
	PatchB(BufferPos+4, wide+CalcWidescreen_size-4);
}

void PatchWideDiv(u32 BufferPos, u32 dstReg)
{
	u32 wide = PatchCopy(CalcWidescreenDiv, CalcWidescreenDiv_size);
	/* Copy original instruction and jump */
	write32(wide, read32(BufferPos));
	PatchB(wide, BufferPos);
	/* Modify destination register */
	write32(wide+0x28, (read32(wide+0x28) & 0xFC1FF83F) | (dstReg << 6) | (dstReg << 21));
	/* Jump back to original code */
	PatchB(BufferPos+4, wide+CalcWidescreenDiv_size-4);
}

void PatchGXWideMulti(u32 BufferPos, u32 dstReg)
{
	u32 wide = PatchCopy(CalcGXWidescreen, CalcGXWidescreen_size);
	/* Copy original instruction and jump */
	write32(wide+CalcGXWidescreen_size-8, read32(BufferPos));
	PatchB(wide, BufferPos);
	/* Jump back to original code */
	PatchB(BufferPos+4, wide+CalcGXWidescreen_size-4);
}

//Credits to Ralf from gc-forever for the original Ocarina Codes

#define FLT_ASPECT_0_913 0x3f69d89c
#define FLT_ASPECT_1_218 0x3f9be5bd

#define FLT_ASPECT_1_200 0x3f99999a
#define FLT_ASPECT_1_600 0x3fcccccd

#define FLT_ASPECT_1_266 0x3fa22222
#define FLT_ASPECT_1_688 0x3fd82d83

#define FLT_ASPECT_1_333 0x3faaaaab
#define FLT_ASPECT_1_777 0x3fe38e39

#define FLT_ASPECT_1_357 0x3fadb6db
#define FLT_ASPECT_1_809 0x3fe79e7a

#define FLT_ASPECT_1_428 0x3fb6db6e
#define FLT_ASPECT_1_905 0x3ff3cf3d

bool PatchWidescreen(u32 FirstVal, u32 Buffer)
{
	if(FirstVal == FLT_ASPECT_0_913 && read32(Buffer+4) == 0x2e736200)
	{
		write32(Buffer, FLT_ASPECT_1_218);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.218] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_200 && (read32(Buffer+4) == 0x43F00000 || 
			(read32(Buffer+4) == 0 && read32(Buffer+8) == 0x43F00000)))
	{	//All Mario Party games share this value
		write32(Buffer, FLT_ASPECT_1_600);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.600] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_266 && read32(Buffer+4) == 0x44180000)
	{
		write32(Buffer, FLT_ASPECT_1_688);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.688] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_333 && (read32(Buffer+4) == 0x481c4000 || 
		read32(Buffer+4) == 0x3f800000 || read32(Buffer+4) == 0xbf800000))
	{
		write32(Buffer, FLT_ASPECT_1_777);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.777] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_357 && read32(Buffer+4) == 0x481c4000)
	{
		write32(Buffer, FLT_ASPECT_1_809);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.809] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_428 && read32(Buffer+4) == 0x3e99999a)
	{
		write32(Buffer, FLT_ASPECT_1_905);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.905] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	return false;
}
extern vu32 TRIGame;
extern u32 IsN64Emu;
bool PatchStaticWidescreen(u32 TitleID, u32 Region)
{
	if(TRIGame == TRI_GP1)
	{
		dbgprintf("PatchWidescreen:[Mario Kart GP1] applied\r\n");
		PatchWideMulti(0x28C800, 1);
		return true;
	}
	else if(TRIGame == TRI_GP2)
	{
		dbgprintf("PatchWidescreen:[Mario Kart GP2] applied\r\n");
		PatchWideMulti(0x2C80D4, 1);
		return true;
	}
	else if(TRIGame == TRI_VS4)
	{
		dbgprintf("PatchWidescreen:[Virtua Striker 4] applied\r\n");
		PatchWideMulti(0x5E418, 0); //clipping
		PatchWideMulti(0x7FA58, 0); //widescreen
		return true;
	}
	else if(TRIGame == TRI_AX)
	{	//thanks CosmoCortney
		dbgprintf("PatchWidescreen:[F-Zero AX] applied\r\n");
		write32(0x445C34, read32(0x445C30));
		return true;
	}
	u32 Buffer, PatchedWide = 0;
	if(IsN64Emu)
	{
		for(Buffer = 0x90000; Buffer < 0x9F000; Buffer+=4)
		{
			if(read32(Buffer) == 0xC3A1005C && read32(Buffer+4) == 0x80030010 && read32(Buffer+8) == 0xFC80E890)
			{
				PatchWideMulti(Buffer, 29); //guPerspective
				PatchedWide++;
			}
			if(read32(Buffer) == 0xC3810014 && read32(Buffer+4) == 0x80030010 && read32(Buffer+8) == 0xFC80E090)
			{
				PatchWideMulti(Buffer, 28); //guPerspectiveF
				PatchedWide++;
			}
		}
		if(PatchedWide)
		{
			dbgprintf("PatchWidescreen:[N64 Emu] applied (%i times)\r\n", PatchedWide);
			return true;
		}
	}
	switch(TitleID)
	{
		case 0x474D34: //Mario Kart Double Dash
			dbgprintf("PatchWidescreen:[Mario Kart Double Dash] applied\r\n");
			if(Region == REGION_ID_USA || Region == REGION_ID_JAP)
			{
				PatchWideMulti(0x1D65A4, 3);
				PatchWideMulti(0x1D65FC, 2);
			}
			else if(Region == REGION_ID_EUR)
			{
				PatchWideMulti(0x1D6558, 3);
				PatchWideMulti(0x1D65B0, 2);
			}
			return true;
		case 0x474145: //Doubutsu no Mori e+
		case 0x474146: //Animal Crossing
			for(Buffer = 0x5E460; Buffer < 0x61EC0; Buffer+=4)
			{	//Every language has its own function location making 7 different locations
				if(read32(Buffer) == 0xFF801090 && read32(Buffer+4) == 0x7C9F2378)
				{
					dbgprintf("PatchWidescreen:[Animal Crossing] applied (0x%08X)\r\n", Buffer);
					PatchWideMulti(Buffer, 28);
					return true;
				}
			}
			return false;
		case 0x47414C: //Super Smash Bros Melee
			for(Buffer = 0x368500; Buffer < 0x36A500; Buffer+=4)
			{
				if(read32(Buffer) == 0x281E0000 && read32(Buffer+4) == 0xC03F0034)
				{
					dbgprintf("PatchWidescreen:[Super Smash Bros Melee] applied (0x%08X)\r\n", Buffer+4);
					PatchWideMulti(Buffer+4, 1);
					return true;
				}
			}
			return false;
		case 0x475445: //1080 Avalanche
			for(Buffer = 0x64000; Buffer < 0x66000; Buffer+=4)
			{
				if(read32(Buffer) == 0xEC000828 && (read32(Buffer+4) == 0xD00302A0 || read32(Buffer+4) == 0xD01C02A0))
				{
					dbgprintf("PatchWidescreen:[1080 Avalanche] applied (0x%08X)\r\n", Buffer);
					PatchWideMulti(Buffer, 0);
					PatchedWide = 1; //patching 2 areas
				}
			}
			return PatchedWide;
		case 0x475049: //Pikmin
			for(Buffer = 0x59000; Buffer < 0x5B000; Buffer+=4)
			{
				if(read32(Buffer) == 0x80BF030C)
				{
					dbgprintf("PatchWidescreen:[Pikmin] applied (0x%08X)\r\n", Buffer);
					write32(Buffer, 0x38A003AC);
					return true;
				}
			}
			return false;
		case 0x475056: //Pikmin 2
			for(Buffer = 0x424000; Buffer < 0x426000; Buffer+=4)
			{
				if(read32(Buffer) == 0xEC011824 && read32(Buffer+12) == 0xC0040000)
				{
					dbgprintf("PatchWidescreen:[Pikmin 2] applied (0x%08X)\r\n", Buffer);
					PatchWideMulti(Buffer, 0);
					return true;
				}
			}
			return false;
		case 0x474D38: //Metroid Prime
		case 0x47324D: //Metroid Prime 2
			for(Buffer = 0x2B0000; Buffer < 0x3B0000; Buffer+=4)
			{
				if(read32(Buffer) == 0xFFA01090 && read32(Buffer+8) == 0xFFC01890 && read32(Buffer+12) == 0xEC250072)
				{
					PatchWideMulti(Buffer, 29); //perspective
					PatchedWide++;
				}
				if(read32(Buffer) == 0xFF401090 && read32(Buffer+4) == 0xEF210032 && read32(Buffer+12) == 0xFFC01890)
				{
					PatchWideMulti(Buffer, 26); //width near plane
					PatchWideMulti(Buffer+12, 30); //width far plane
					PatchedWide++;
				}
			}
			dbgprintf("PatchWidescreen:[Metroid Prime] applied (%i times)\r\n", PatchedWide);
			return PatchedWide;
		case 0x474832: //Need for Speed Hot Pursuit 2
			dbgprintf("PatchWidescreen:[Need for Speed Hot Pursuit 2] applied\r\n");
			if(Region == REGION_ID_USA)
			{
				write32(0x14382C, 0xC0429AE8);
				write32(0x143D58, 0xC0029AE8);
			}
			else if(Region == REGION_ID_EUR)
			{
				write32(0x1439AC, 0xC0429AE8);
				write32(0x143ED8, 0xC0029AE8);
			}
			return true;
		case 0x474C4D: //Luigis Mansion
			dbgprintf("PatchWidescreen:[Luigis Mansion] applied\r\n");
			if(Region == REGION_ID_USA)
				PatchWideMulti(0x206A4, 0);
			else if(Region == REGION_ID_EUR)
				PatchWideMulti(0x2143C, 0);
			else if(Region == REGION_ID_JAP)
				PatchWideMulti(0x20300, 0);
			return true;
		case 0x474342: //Crash Bandicoot
			dbgprintf("PatchWidescreen:[Crash Bandicoot] clipping applied\r\n");
			if(Region == REGION_ID_USA)
				write32(0xAC768, 0xD01E0040);
			else if(Region == REGION_ID_EUR)
				write32(0xAC9A4, 0xD01E0040);
			else if(Region == REGION_ID_JAP)
				write32(0xADF1C, 0xD01E0040);
			return false; //aspect ratio gets patched later
		case 0x473258: //Sonic Gems Collection
			if(Region == REGION_ID_USA)
			{
				if(read32(0x48A2A4) == FLT_ASPECT_1_200)
				{
					dbgprintf("PatchWidescreen:[Sonic R] applied\r\n");
					//aspect ratio
					write32(0x48A2A4,FLT_ASPECT_1_600);
					//free space for new wide values
					write32(0x69124,0x7C69586E);
					write32(0x6912C,0x7E631A14);
					//general clipping left half
					write32(0x69134,0x3800FF96);
					//general clipping right half
					write32(0x69138,0x3A4002E9);
					//stage clipping multiplier
					PatchWideMulti(0x87CAC, 13);
					return true;
				}
			}
			else if(Region == REGION_ID_EUR)
			{
				if(read32(0x48A364) == FLT_ASPECT_1_200)
				{
					dbgprintf("PatchWidescreen:[Sonic R] applied\r\n");
					//aspect ratio
					write32(0x48A364,FLT_ASPECT_1_600);
					//free space for new wide values
					write32(0x69128,0x7C69586E);
					write32(0x69130,0x7E631A14);
					//general clipping left half
					write32(0x69138,0x3800FF96);
					//general clipping right half
					write32(0x6913C,0x3A4002E9);
					//stage clipping multiplier
					PatchWideMulti(0x87CB4, 13);
					return true;
				}
			}
			else if(Region == REGION_ID_JAP)
			{
				if(read32(0x48A104) == FLT_ASPECT_1_200)
				{
					dbgprintf("PatchWidescreen:[Sonic R] applied\r\n");
					//aspect ratio
					write32(0x48A104,FLT_ASPECT_1_600);
					//free space for new wide values
					write32(0x69124,0x7C69586E);
					write32(0x6912C,0x7E631A14);
					//general clipping left half
					write32(0x69134,0x3800FF96);
					//general clipping right half
					write32(0x69138,0x3A4002E9);
					//stage clipping multiplier
					PatchWideMulti(0x87BA0, 13);
					return true;
				}
			}
			return false;
		case 0x474E46: //NFL Blitz 2002
			if(Region == REGION_ID_USA)
			{
				dbgprintf("PatchWidescreen:[NFL Blitz 2002] applied\r\n");
				write32(0x199B3C,0xC0040000);
				PatchWideMulti(0x199B3C,0);
				write32(0x199B44,0xD01F0068);
				return true;
			}
			return false;
		case 0x474F33: //NFL Blitz 2003
			if(Region == REGION_ID_USA)
			{
				dbgprintf("PatchWidescreen:[NFL Blitz 2003] applied\r\n");
				write32(0x2037A8,0xC01D0000);
				PatchWideMulti(0x2037A8,0);
				write32(0x2037B0,0xD01F0068);
				return true;
			}
			return false;
		case 0x474656: //NFL Blitz Pro
			if(Region == REGION_ID_USA)
			{
				dbgprintf("PatchWidescreen:[NFL Blitz Pro] applied\r\n");
				write32(0x274114,0xC0040000);
				PatchWideMulti(0x274114,0);
				write32(0x27412C,0xD0030068);
				return true;
			}
			return false;
		case 0x474541: //skies of arcadia legends
			dbgprintf("PatchWidescreen:[Skies of Arcadia Legends] applied\r\n");
			if(Region == REGION_ID_USA) {
				PatchWideMulti(0x1DCE00,2); //picture effects
				PatchWideDiv(0x299334,6); //widescreen, clipping
			} else if(Region == REGION_ID_EUR) {
				PatchWideMulti(0x1DEADC,2); //picture effects
				PatchWideDiv(0x29BC5C,6); //widescreen, clipping
			} else if(Region == REGION_ID_JAP) {
				PatchWideMulti(0x1DCCDC,1); //picture effects
				PatchWideDiv(0x298DE4,6); //widescreen, clipping
			}
			return true;
		default:
			return false;
	}
}
