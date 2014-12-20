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

#ifndef __DIP_H__
#define __DIP_H__

#include <gctypes.h>

#define		DI_BASE		0xD3026000

#define		DI_STATUS	(*(vu32*)(DI_BASE+0x00))
#define		DI_COVER	(*(vu32*)(DI_BASE+0x04))
#define		DI_CMD_0	(*(vu32*)(DI_BASE+0x08))
#define		DI_CMD_1	(*(vu32*)(DI_BASE+0x0C))
#define		DI_CMD_2	(*(vu32*)(DI_BASE+0x10))
#define		DI_DMA_ADR	(*(vu32*)(DI_BASE+0x14))
#define		DI_DMA_LEN	(*(vu32*)(DI_BASE+0x18))
#define		DI_CONTROL	(*(vu32*)(DI_BASE+0x1C))
#define		DI_IMM		(*(vu32*)(DI_BASE+0x20))
#define		DI_CONFIG	(*(vu32*)(DI_BASE+0x24))

#define		DI_SHADOW	(DI_BASE + 0x30)

#define		DI_SSTATUS	(*(vu32*)(DI_SHADOW+0x00))
#define		DI_SCOVER	(*(vu32*)(DI_SHADOW+0x04))
#define		DI_SCMD_0	(*(vu32*)(DI_SHADOW+0x08))
#define		DI_SCMD_1	(*(vu32*)(DI_SHADOW+0x0C))
#define		DI_SCMD_2	(*(vu32*)(DI_SHADOW+0x10))
#define		DI_SDMA_ADR	(*(vu32*)(DI_SHADOW+0x14))
#define		DI_SDMA_LEN	(*(vu32*)(DI_SHADOW+0x18))
#define		DI_SCONTROL	(*(vu32*)(DI_SHADOW+0x1C))
#define		DI_SIMM		(*(vu32*)(DI_SHADOW+0x20))
#define		DI_SCONFIG	(*(vu32*)(DI_SHADOW+0x24))

#define		DIP_BASE	0xCD806000

#define		DIP_STATUS	(DIP_BASE+0x00)
#define		DIP_COVER	(DIP_BASE+0x04)
#define		DIP_CMD_0	(DIP_BASE+0x08)
#define		DIP_CMD_1	(DIP_BASE+0x0C)
#define		DIP_CMD_2	(DIP_BASE+0x10)
#define		DIP_DMA_ADR	(DIP_BASE+0x14)
#define		DIP_DMA_LEN	(DIP_BASE+0x18)
#define		DIP_CONTROL	(DIP_BASE+0x1C)
#define		DIP_IMM		(DIP_BASE+0x20)
#define		DIP_CONFIG	(DIP_BASE+0x24)

#define		DIP_CMD_NORMAL	0xA8
#define		DIP_CMD_DVDR	0xD0

void ReadRealDisc(u8 *Buffer, u32 Offset, u32 Length, u32 Command);
void DVDStartCache(void);

#endif