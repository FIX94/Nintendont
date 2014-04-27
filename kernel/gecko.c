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
#include "gecko.h"

void EXISendByte( char byte )
{

loop:
	*(vu32*)0xD806814			= 0xD0;
	*(vu32*)(0xD806814+0x10)	= 0xB0000000 | (byte<<20);
	*(vu32*)(0xD806814+0x0C)	= 0x19;

	while( *(vu32*)(0xD806814+0x0C)&1 );

	u32 loop =  *(vu32*)(0xD806814+0x10)&0x4000000;
	
	*(vu32*)0xD806814	= 0;

	if( !loop )
		goto loop;

	return;
}
void GeckoSendBuffer( char *buffer )
{
	int i = 0;
	while( buffer[i] != '\0' )
	{
		EXISendByte( buffer[i] );
		++i;
	}

	return;
}