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
#ifndef _PATCHWIDESCREEN_H_
#define _PATCHWIDESCREEN_H_

bool PatchWidescreen(u32 FirstVal, u32 Buffer);
void PatchWideMulti(u32 BufferPos, u32 dstReg);

/**
 * Apply a static widescreen patch.
 * @param TitleID Game ID, rshifted by 8.
 * @param Region Region byte from Game ID.
 * @return True if a patch was applied; false if not.
 */
bool PatchStaticWidescreen(u32 TitleID, u32 Region);

#endif
