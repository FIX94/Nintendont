/*
MemCard.h for Nintendont (Kernel)

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

#ifndef __MEMCARD_H__
#define __MEMCARD_H__

/**
 * Create a blank memory card image.
 * @param MemCard Memory card filename.
 * @param BI2region bi2.bin region code.
 * @return True on success; false on error.
 */
bool GenerateMemCard(const char *MemCard, u32 BI2region);

#endif
