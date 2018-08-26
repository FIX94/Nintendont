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
#include "utils.h"
#include "global.h"
#include "cache.h"

void RAMInit(void)
{
	u32 vmode = *(vu32*)0x800000CC;

	_memset( (void*)0x80000020, 0, 0xE0 ); //Keep ISO Header
	_memset( (void*)0x80003000, 0, 0x12FD000 );
	sync_before_exec( (void*)0x80003000, 0x12FD000 );
	_memset( (void*)0x81310000, 0, 0x4F0000 );
	sync_before_exec( (void*)0x81340000, 0x4F0000 );
	*(vu32*)0x80000020 = 0x0D15EA5E;
	*(vu32*)0x80000028 = 0x01800000;
	*(vu32*)0x8000002C = *(vu32*)0xCC00302C >> 28;		// console type
	*(vu32*)0x800000CC = vmode;			//Assuming it didnt change
	*(vu32*)0x800000F0 = 0x01800000;

	*(vu16*)0xCC00501A = 156;
}
