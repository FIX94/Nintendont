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

#define REGION_USA 0x45
#define REGION_JAP 0x4A
#define REGION_EUR 0x50

//Credits to Ralf from gc-forever for the original Ocarina Codes
bool PatchStaticWidescreen(u32 TitleID, u32 Region)
{
	switch(TitleID)
	{
		case 0x474D34: //Mario Kart Double Dash
			if(Region == REGION_USA || Region == REGION_JAP)
			{
				PatchWideMulti(0x1D65A4, 3);
				PatchWideMulti(0x1D65FC, 2);
			}
			else if(Region == REGION_EUR)
			{
				PatchWideMulti(0x1D6558, 3);
				PatchWideMulti(0x1D65B0, 2);
			}
			return true;
		case 0x475A4C: //Wind Waker
			if(Region == REGION_USA)
				write32(0x3FA998, 0x3FE38E39);
			else if(Region == REGION_EUR)
				write32(0x4021C0, 0x3FE38E39);
			else if(Region == REGION_JAP)
				write32(0x3EDE50, 0x3FE38E39);
			return true;
		case 0x475A32: //Twilight Princess
			if(Region == REGION_USA)
				write32(0x45391C, 0x3FE79E7A);
			else if(Region == REGION_EUR)
				write32(0x4558DC, 0x3FE79E7A);
			else if(Region == REGION_JAP)
				write32(0x44DA64, 0x3FE79E7A);
			return true;
		case 0x474D53: //Super Mario Sunshine
			if(Region == REGION_USA)
				write32(0x416B74, 0x3F9BE5BD);
			else if(Region == REGION_EUR)
				write32(0x40E0D4, 0x3F9BE5BD);
			else
				return false;
			return true;
		default:
			return false;
	}
}
