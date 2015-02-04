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
		dbgprintf("Patch:[Aspect Ratio 1.218] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_200 && (read32(Buffer+4) == 0x43F00000 || 
			(read32(Buffer+4) == 0 && read32(Buffer+8) == 0x43F00000)))
	{	//All Mario Party games share this value
		write32(Buffer, FLT_ASPECT_1_600);
		dbgprintf("Patch:[Aspect Ratio 1.600] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_266 && read32(Buffer+4) == 0x44180000)
	{
		write32(Buffer, FLT_ASPECT_1_688);
		dbgprintf("Patch:[Aspect Ratio 1.688] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_333 && (read32(Buffer+4) == 0x481c4000 || read32(Buffer+4) == 0xbf800000))
	{
		write32(Buffer, FLT_ASPECT_1_777);
		dbgprintf("Patch:[Aspect Ratio 1.777] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_357 && read32(Buffer+4) == 0x481c4000)
	{
		write32(Buffer, FLT_ASPECT_1_809);
		dbgprintf("Patch:[Aspect Ratio 1.809] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_428 && read32(Buffer+4) == 0x3e99999a)
	{
		write32(Buffer, FLT_ASPECT_1_905);
		dbgprintf("Patch:[Aspect Ratio 1.905] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	return false;
}
extern vu32 TRIGame;
bool PatchStaticWidescreen(u32 TitleID, u32 Region)
{
	if(TRIGame == TRI_AX)
	{	//thanks CosmoCortney
		dbgprintf("Patch:F-Zero AX Widescreen\r\n");
		write32(0x445C34, read32(0x445C30));
		return true;
	}
	u32 Buffer, PatchedWide = 0;
	switch(TitleID)
	{
		case 0x474D34: //Mario Kart Double Dash
			dbgprintf("Patch:Patched MKDD Widescreen\r\n");
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
					dbgprintf("Patch:Patched Animal Crossing Widescreen (0x%08X)\r\n", Buffer);
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
					dbgprintf("Patch:Patched SSBM Widescreen (0x%08X)\r\n", Buffer+4);
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
					dbgprintf("Patch:Patched 1080 Avalanche Widescreen (0x%08X)\r\n", Buffer);
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
					dbgprintf("Patch:Patched Pikmin Widescreen (0x%08X)\r\n", Buffer);
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
					dbgprintf("Patch:Patched Pikmin 2 Widescreen (0x%08X)\r\n", Buffer);
					PatchWideMulti(Buffer, 0);
					return true;
				}
			}
			return false;
		case 0x474832: //Need for Speed Hot Pursuit 2
			dbgprintf("Patch:Patched NFS:HP2 Widescreen\r\n");
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
			dbgprintf("Patch:Patched Luigis Mansion Widescreen\r\n");
			if(Region == REGION_ID_USA)
				PatchWideMulti(0x206A4, 0);
			else if(Region == REGION_ID_EUR)
				PatchWideMulti(0x2143C, 0);
			else if(Region == REGION_ID_JAP)
				PatchWideMulti(0x20300, 0);
			return true;
		case 0x474342: //Crash Bandicoot
			dbgprintf("Patch:Patched Crash Bandicoot Clipping\r\n");
			if(Region == REGION_ID_USA)
				write32(0xAC768, 0xD01E0040);
			else if(Region == REGION_ID_EUR)
				write32(0xAC9A4, 0xD01E0040);
			else if(Region == REGION_ID_JAP)
				write32(0xADF1C, 0xD01E0040);
			return false; //aspect ratio gets patched later
		default:
			return false;
	}
}
