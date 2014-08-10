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

#define		DI_BASE		0xC0002F00

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

int DVDLowRead( void *ptr, u32 len, u32 offset );

#endif
