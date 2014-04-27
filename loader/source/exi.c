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
#include "global.h"
#include "exi.h"
#include <stdio.h>

static FILE *nl_log = NULL;
static u32 GeckoFound = 0;

void CheckForGecko( void )
{
	if( !IsWiiU() )
		GeckoFound = usb_isgeckoalive( 1 );
}

void EXISendByte( char byte )
{

loop:
	*(vu32*)EXI			= 0xD0;
	*(vu32*)(EXI+0x10)	= 0xB0000000 | (byte<<20);
	*(vu32*)(EXI+0x0C)	= 0x19;

	while( *(vu32*)(EXI+0x0C)&1 );

	u32 loop =  *(vu32*)(EXI+0x10)&0x4000000;
	
	*(vu32*)EXI	= 0;

	if( !loop )
		goto loop;

	return;
}

int gprintf( const char *str, ... )
{
	if( IsWiiU() )
	{
		// We're running on a vWii, log the results to a file
		
		// Open the file if it hasn't been alread
		if (nl_log == NULL)
		{
			char LogPath[20];
			sprintf(LogPath, "%s:/nloader.log", GetRootDevice());
			nl_log = fopen(LogPath, "a");
		}
		if (nl_log != NULL)
		{
			va_list ap;
			va_start(ap,str);
			vfprintf(nl_log, str, ap); // No need for a buffer, goes straight to the file
			// Flushes the stream so we don't have to wait for the file to close or it to fill
			fflush(nl_log);
			va_end(ap);
		} else {
			return -1; // Couldn't open the file
		}
	} else {
		// We're running on a real Wii, send the results to a USB Gecko
		if(!GeckoFound)
			return 0; // No USB Gecko found

		char astr[4096] = {0};

		va_list ap;
		va_start(ap,str);
		vsnprintf(astr, sizeof(astr), str, ap);
		va_end(ap);
		int i = 0;
		while( astr[i] != '\0' )
		{
			EXISendByte( astr[i] );
			++i;
		}
	}

	return 1; // Everything went okay
}