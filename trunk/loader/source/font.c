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
#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <ctype.h>
#include <unistd.h>

#include "font.h"
#include "wii-font.h"

static void PrintChar( int xx, int yy, char c )
{
	unsigned long* fb = (unsigned long*)VIDEO_GetCurrentFramebuffer();

	if( fb == NULL )
		return;

	if( c >= 0x7F || c < 0x20)
		c = ' ';
	int x,y;
	for(  x=1; x <7; ++x)
	{
		for(  y=0; y<16; ++y)
		{
			fb[(x+xx)+(y+yy)*320] = WiiFont[x+(y+(c-' ')*16)*8];
		}
	}
}

static inline void PrintString( int x, int y, char *str )
{
	int i=0;
	while(str[i]!='\0')
	{
		PrintChar( x+i*6, y, str[i] );
		i++;
	}
}

void PrintFormat( int x, int y, const char *str, ... )
{
	char astr[2048] = {0};

	va_list ap;
	va_start( ap, str );

	vsnprintf(astr, sizeof(astr), str, ap);

	va_end( ap );

	PrintString( x, y, astr );
}
