/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

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
#include "NAND.h"

extern u8  *CNTMap;

u8 *NANDLoadFile( char *path, u32 *Size )
{
	s32 fd = IOS_Open( path, 1 );
	if( fd < 0 )
	{
		//dbgprintf("ES:NANDLoadFile->IOS_Open(\"%s\", 1 ):%d\n", path, fd );
		*Size = fd;
		return (u8*)NULL;
	}
	//dbgprintf("ES:NANDLoadFile->IOS_Open(\"%s\", 1 ):%d\n", path, fd );

	*Size = IOS_Seek( fd, 0, SEEK_END );
	//dbgprintf("ES:NANDLoadFile->Size:%d\n", *Size );

	IOS_Seek( fd, 0, 0 );

	u8 *data = (u8*)heap_alloc_aligned( 0, *Size, 0x40 );
	if( data == NULL )
	{
		//dbgprintf("ES:NANDLoadFile(\"%s\")->Failed to alloc %d bytes!\n", path, status->Size );
		IOS_Close( fd );
		return (u8*)NULL;
	}

	s32 r = IOS_Read( fd, data, *Size );
	//dbgprintf("ES:NANDLoadFile->IOS_Read():%d\n", r );
	if( r < 0 )
	{
		//dbgprintf("ES:NANDLoadFile->IOS_Read():%d\n", r );
		*Size = r;
		IOS_Close( fd );
		return (u8*)NULL;
	}

	IOS_Close( fd );

	return data;
}
