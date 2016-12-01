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
#include <ogc/machine/processor.h>
#include <unistd.h>
#include "dip.h"
#include "global.h"

void DVDStartCache(void)
{
	DI_CMD_0	= 0xF9000000;
	DI_CONTROL = 1;
	while( DI_CONTROL & 1 );
}

//Use same buffer as kernel, uncached though, to see howto cache see DI.c
static u8 *const DISC_DRIVE_BUFFER = (u8*)0x92000800;
//static const u32 DISC_DRIVE_BUFFER_LENGTH = 0x7FF000;
void ReadRealDisc(u8 *Buffer, u64 Offset, u32 Length, u32 Command)
{
	u32 ReadDiff = 0;

	u32 TmpLen = Length;
	u64 TmpOffset = Offset;

	write32(DIP_STATUS, 0x54); //mask and clear interrupts

	//Actually read
	if (Command == DIP_CMD_DVDR)
	{
		// Adjust length and offset for DVD-R mode.
		TmpOffset = ALIGN_BACKWARD(Offset, 0x800);
		ReadDiff = (u32)(Offset - TmpOffset);
		TmpLen = ALIGN_FORWARD(TmpLen + ReadDiff, 0x800);

		write32(DIP_CMD_0, DIP_CMD_DVDR << 24);
		write32(DIP_CMD_1, (u32)(TmpOffset >> 11));
		write32(DIP_CMD_2, TmpLen >> 11);
	}
	else
	{
		write32(DIP_CMD_0, DIP_CMD_NORMAL << 24);
		write32(DIP_CMD_1, (u32)(TmpOffset >> 2));
		write32(DIP_CMD_2, TmpLen);
	}

	DCInvalidateRange(DISC_DRIVE_BUFFER, TmpLen);
	write32(DIP_DMA_ADR, (u32)DISC_DRIVE_BUFFER);
	write32(DIP_DMA_LEN, TmpLen);

	write32( DIP_CONTROL, 3 );
	usleep(70);

	while(read32(DIP_CONTROL) & 1)
		usleep(200);

	write32(DIP_STATUS, 0x54); //mask and clear interrupts
	usleep(70);

	memcpy(Buffer, DISC_DRIVE_BUFFER + ReadDiff, Length);
}
