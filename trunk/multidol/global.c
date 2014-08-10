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

void RAMInit(void)
{
	u32 vmode = *(vu32*)0x800000CC;

	_memset( (void*)0x80000020, 0, 0xE0 ); //Keep ISO Header
	_memset( (void*)0x80003000, 0, 0x100 );
	_memset( (void*)0x80003F00, 0, 0x11FC100 );
	_memset( (void*)0x81340000, 0, 0x3C );

	*(vu32*)0x80000028 = 0x01800000;
	*(vu32*)0x8000002C = *(vu32*)0xCC00302C >> 28;		// console type
	*(vu32*)0x80000038 = 0x01800000;
	*(vu32*)0x800000CC = vmode;			//Assuming it didnt change
	*(vu32*)0x800000F0 = 0x01800000;
	*(vu32*)0x800000EC = 0x81800000;

	*(vu32*)0x80003100 = 0x01800000;	//Physical MEM1 size 
	*(vu32*)0x80003104 = 0x01800000;	//Simulated MEM1 size 
	*(vu32*)0x80003108 = 0x01800000;
	*(vu32*)0x8000310C = 0;
	*(vu32*)0x80003110 = 0;
	*(vu32*)0x80003114 = 0;
	*(vu32*)0x80003118 = 0;				//Physical MEM2 size 
	*(vu32*)0x8000311C = 0;				//Simulated MEM2 size 
	*(vu32*)0x80003120 = 0;
	*(vu32*)0x80003124 = 0x0000FFFF;
	*(vu32*)0x80003128 = 0;
	*(vu32*)0x80003130 = 0x0000FFFF;	//IOS Heap Range 
	*(vu32*)0x80003134 = 0;
	*(vu32*)0x80003138 = 0x11;			//Hollywood Version 
	*(vu32*)0x8000313C = 0;

	*(vu32*)0x8000315C = 0x81;

	*(vu16*)0xCC00501A = 156;
}
