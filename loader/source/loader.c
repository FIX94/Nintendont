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
#include "loader.h"
#include "global.h"
#include "exi.h"
#include "font.h"
#include "dip.h"
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ogc/cache.h>

int dummy( const char *str, ... )
{
	return 0;
}

u32 LoadGame( void )
{
	void	(*app_init)(int (*report)(const char *fmt, ...));
	int		(*app_main)(char **dst, u32 *size, u32 *offset);
	void	(*app_entry)(void (**init)(int (*report)(const char *fmt, ...)), int (**main)(), void *(**final)());
	void	(*entrypoint)();
	void *	(*app_final)();

	s32 ret = DVDLowRead( (void*)0x80000000, 0x20, 0 );
	if( !ret )
	{
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Fatal DVDLowRead() failed");
		ExitToLoader(0);
	}

	u8 *AppInfo = (u8 *)memalign( 32, 0x20 );

	memset( AppInfo, 0, 32 );

	ret = DVDLowRead( (void*)AppInfo, 32, 0x2440 );
	if( !ret )
	{
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Fatal DVDLowRead() failed");
		ExitToLoader(0);
	}
	
#ifdef DEBUG
	gprintf("AppLoader Size:%08X\n", *(vu32*)(AppInfo+0x14) +  *(vu32*)(AppInfo+0x18) );
#endif

	ret = DVDLowRead( (void*)0x81200000, *(vu32*)(AppInfo+0x14) + *(vu32*)(AppInfo+0x18), 0x2460 );
	if( !ret )
	{
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Fatal DVDLowRead() failed");
		ExitToLoader(0);
	}

	ICInvalidateRange( (void*)0x81200000, ((*(vu32*)(AppInfo+0x14) + *(vu32*)(AppInfo+0x18)) + 31) & (~31) );	
	
	*(vu32*)0x800000F8 = 243000000;				// Bus Clock Speed
	*(vu32*)0x800000FC = 729000000;				// CPU Clock Speed		

	app_entry = ((void*)(*(vu32*)(AppInfo+0x10)));
#ifdef DEBUG
	gprintf("Apploader Entry:%p\n", app_entry );
#endif
	
	if( (u32)app_entry < 0x81200000 || (u32)app_entry > 0x81300000 )
	{
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Fatal error app_entry is not within the apploader area!");
		ExitToLoader(0);
	}

	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*18, "Apploader OK");

	app_entry( &app_init, &app_main, &app_final );

#ifdef DEBUG
	gprintf("Apploader Init: %p\n", app_init);
	gprintf("Apploader Main: %p\n", app_main);
	gprintf("Apploader Final:%p\n", app_final);
#endif

	if( IsWiiU() )
		app_init(dummy);
	else
		app_init(gprintf);
	
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*19, "Loading main.dol");

	u32 counter = 13;

	while (1)
	{
		char *Data = NULL;
		u32 Length = 0;
		u32 Offset = 0;
		
		PrintFormat( MENU_POS_X + counter * 8, MENU_POS_Y + 20*19, ".");

		if( app_main( &Data, &Length, &Offset ) != 1 )
			break;
		
		if( (u32)Data < 0x80000000 || (u32)Data >= 0x81800000 )
		{
#ifdef DEBUG
			gprintf("app_main(dst:%p)\n", Data );
#endif
			PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Fatal error app_main:Data not in MEM1!");
			ExitToLoader(0);
		}
#ifdef DEBUG
		gprintf("Game:RunGame->DVDLowRead( %p, %08x, %08x)\n", Data, Length, Offset );
#endif

		ret = DVDLowRead( Data, Length, Offset );
		if( !ret )
		{
			PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Fatal DVDLowRead() failed");
			ExitToLoader(0);
		}

		if( Offset == 0x440 && Length == 0x20 )
		{
			Region = *(vu32*)(Data+0x18);
#ifdef DEBUG
			gprintf("Game:Region:%u\n", Region );
#endif
		}
		counter++;
	}
	
	entrypoint = app_final();
	
#ifdef DEBUG
	gprintf("Game:RunGame->entrypoint(%x)\n", entrypoint );
#endif
	
	if( (u32)entrypoint < 0x80000000 || (u32)app_entry >= 0x81800000 )
	{
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Fatal error entrypoint is not within MEM1!");
		ExitToLoader(0);
	}

	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*10, "Loading complete\n");

	return (u32)entrypoint;
}
