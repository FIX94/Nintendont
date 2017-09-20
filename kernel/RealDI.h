/*
RealDI.h for Nintendont (Kernel)

Copyright (C) 2015 FIX94

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

#ifndef _REALDI_H_
#define _REALDI_H_

#define		DIP_CMD_NORMAL	0xA8
#define		DIP_CMD_DVDR	0xD0

void RealDI_Init();
void RealDI_Update();
void RealDI_Identify(bool NeedsGC);
bool RealDI_NewDisc();
void ClearRealDiscBuffer(void);
const u8 *ReadRealDisc(u32 *Length, u32 Offset, bool NeedSync);

#endif
