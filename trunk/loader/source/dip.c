/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

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
#include "dip.h"
#include "ogc/cache.h"
#include "global.h"
#include <string.h>
#include <ogc/machine/processor.h>
void DVDStartCache(void)
{
	DI_CMD_0	= 0xF9000000;
	DI_CONTROL = 1;
	while( DI_CONTROL & 1 );
}

void ReadRealDisc(u8 *Buffer, u32 Offset, u32 Length, u32 Command)
{
	write32(DIP_STATUS, 0x54); //mask and clear interrupts

	//Actually read
	write32(DIP_CMD_0, Command << 24);
	write32(DIP_CMD_1, Command == DIP_CMD_DVDR ? ALIGN_BACKWARD(Offset,0x800) >> 11 : Offset >> 2);
	write32(DIP_CMD_2, Command == DIP_CMD_DVDR ? ALIGN_FORWARD(Length,0x800) >> 11 : Length);

	DCInvalidateRange( Buffer, Length );
	write32(DIP_DMA_ADR, (u32)Buffer);
	write32(DIP_DMA_LEN, Command == DIP_CMD_DVDR ? ALIGN_FORWARD(Length,0x800) : Length);

	write32(DIP_CONTROL, 3);
	while(read32(DIP_CONTROL) & 1);
}
