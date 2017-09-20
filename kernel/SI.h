/*
SI.h for Nintendont (Kernel)

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
#ifndef __SI_H__
#define __SI_H__

#define		SI_BASE		0x13026400

#define		SI_CHAN_0	(SI_BASE+0x00)
#define		SI_CHAN_1	(SI_BASE+0x0C)
#define		SI_CHAN_2	(SI_BASE+0x18)
#define		SI_CHAN_3	(SI_BASE+0x24)
#define		SI_POLL		(SI_BASE+0x30)
#define		SI_CONTROL	(SI_BASE+0x34)
#define		SI_STATUS	(SI_BASE+0x38)
#define		SI_EXI_LOCK	(SI_BASE+0x3C)
#define		SI_IO_BUF	(SI_BASE+0x80)

#define		PAD_BUFF	0x13003100

void SIInit();
void SIInterrupt();
void SIUpdateRegisters();

#endif
