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
#include "menu.h"
#include "font.h"
#include "exi.h"
#include "global.h"
#include "FPad.h"
#include "Config.h"
#include <dirent.h>
#include <sys/dir.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ogc/stm.h>
#include <ogc/video.h>
#include <ogc/video_types.h>
#include <ogc/consol.h>
#include <ogc/system.h>
#include <fat.h>

extern NIN_CFG* ncfg;

u32 Shutdown = 0;
void HandleWiiMoteEvent(s32 chan)
{
	Shutdown = 1;
}
void HandleSTMEvent(u32 event)
{
	*(vu32*)(0xCC003024) = 1;

	switch(event)
	{
		default:
		case STM_EVENT_RESET:
			break;
		case STM_EVENT_POWER:
			Shutdown = 1;
			break;
	}
}
void SelectGame( void )
{
//Create a list of games
	char filename[MAXPATHLEN];
	char gamename[MAXPATHLEN];

	DIR *pdir;
	struct dirent *pent;
	struct stat statbuf;

	sprintf(filename, "%s:/games", GetRootDevice());
	pdir = opendir(filename);
	if( !pdir )
	{
		ClearScreen();
		gprintf("No FAT device found, or missing %s dir!\n", filename);
		PrintFormat( 25, 232, "No FAT device found, or missing %s dir!", filename );
		ExitToLoader(1);
	}

	u32 gamecount = 0;
	char buf[0x100];
	gameinfo gi[MAX_GAMES];

	memset( gi, 0, sizeof(gameinfo) * MAX_GAMES );

	while( ( pent = readdir(pdir) ) != NULL )
	{
		stat( pent->d_name, &statbuf );
		if( pent->d_type == DT_DIR )
		{
			if( pent->d_name[0] == '.' )	//skip current and previous directories
				continue;

		//	gprintf( "%s", pent->d_name );

			//Test if game.iso exists and add to list

			bool found = false;
			u32 DiscNumber;
			for (DiscNumber = 0; DiscNumber < 2; DiscNumber++)
			{
				sprintf( filename, "%s:/games/%s/%s.iso", GetRootDevice(), pent->d_name, DiscNumber ? "disc2" : "game" );

				FILE *in = fopen( filename, "rb" );
				if( in != NULL )
				{
				//	gprintf("(%s) ok\n", filename );
					fread( buf, 1, 0x100, in );
					fclose(in);

					if( *(vu32*)(buf+0x1C) == 0xC2339F3D )	// Must be GC game
					{
						memcpy(gi[gamecount].ID, buf, 4); //ID for EXI
						strcpy( gamename, buf + 0x20 );
						if (DiscNumber)
							strcat( gamename, " (2)" );
						gi[gamecount].Name = strdup( gamename );
						gi[gamecount].Path = strdup( filename );

						gamecount++;
						found = true;
					}
				}
			}
			if ( !found ) // Check for FST format
			{
				sprintf(filename, "%s:/games/%s/sys/boot.bin", GetRootDevice(), pent->d_name);

				FILE *in = fopen( filename, "rb" );
				if( in != NULL )
				{
				//	gprintf("(%s) ok\n", filename );
					fread( buf, 1, 0x100, in );
					fclose(in);

					if( *(vu32*)(buf+0x1C) == 0xC2339F3D )	// Must be GC game
					{
						sprintf(filename, "%s:/games/%s/", GetRootDevice(), pent->d_name);

						memcpy(gi[gamecount].ID, buf, 4); //ID for EXI
						gi[gamecount].Name = strdup( buf + 0x20 );
						gi[gamecount].Path = strdup( filename );

						gamecount++;
					}
				}
			}
		}
	}

	if( gamecount == 0 )
	{
		ClearScreen();
		gprintf("No games found in %s:/games !\n", GetRootDevice());
		PrintFormat( 25, 232, "No games found in %s:/games !", GetRootDevice() );
		ExitToLoader(1);
	}

	u32 redraw = 1;
	u32 i;
	s32 PosX = 0;
	s32 ScrollX = 0;
	u32 MenuMode = 0;

	u32 ListMax = gamecount;
	if( ListMax > 14 )
		ListMax = 14;
	bool SaveSettings = false;

	while(1)
	{
		PrintInfo();
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*1, "Home: Exit");
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*2, "A   : %s", MenuMode ? "Modify" : "Select");
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*3, "B   : %s", MenuMode ? "Game List" : "Settings ");

		FPAD_Update();

		if( FPAD_Start(1) )
		{
			ClearScreen();
			PrintFormat( 90, 232, "Returning to loader..." );
			ExitToLoader(0);
		}

		if( FPAD_Cancel(0) )
		{
			MenuMode ^= 1;

			PosX	= 0;
			ScrollX = 0;

			if( MenuMode == 0 )
			{
				ListMax = gamecount;
				if( ListMax > 14 )
					ListMax = 14;

			}  else {

				ListMax = 13;

				if( (ncfg->VideoMode & NIN_VID_MASK) == NIN_VID_FORCE )
					ListMax = 14;
			}
			
			redraw = 1;
			SaveSettings = true;

			ClearScreen();
		}

	//	gprintf("\rS:%u P:%u G:%u M:%u    ", ScrollX, PosX, gamecount, ListMax );

		if( MenuMode == 0 )		//game select menu
		{
			if( FPAD_Down(0) )
			{
				PrintFormat( MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

				if( PosX + 1 >= ListMax )
				{
					if( PosX + 1 + ScrollX < gamecount)
						ScrollX++;
					else {
						PosX	= 0;
						ScrollX = 0;
					}
				} else {
					PosX++;
				}
			
				redraw=1;
			} else if( FPAD_Up(0) )
			{
				PrintFormat( MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

				if( PosX <= 0 )
				{
					if( ScrollX > 0 )
						ScrollX--;
					else {
						PosX	= ListMax - 1;
						ScrollX = gamecount - ListMax;
					}
				} else {
					PosX--;
				}

				redraw=1;
			}

			if( FPAD_OK(0) )
			{
				break;
			}

			if( redraw )
			{
				for( i=0; i < ListMax; ++i )
					PrintFormat( MENU_POS_X-8, MENU_POS_Y + 20*6 + i * 20, "% 51.51s", gi[i+ScrollX].Name );
					
				PrintFormat( MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, "<" );
			}

		} else {	//settings menu		
			
			if( FPAD_Down(0) )
			{
				PrintFormat( MENU_POS_X+30, 164+16*PosX, " " );
				
				PosX++;

				if( PosX >= ListMax )
				{
					ScrollX = 0;
					PosX	= 0;					
				}
			
				redraw=1;

			} else if( FPAD_Up(0) )
			{
				PrintFormat( MENU_POS_X+30, 164+16*PosX, " " );

				PosX--;

				if( PosX < 0 )
					PosX = ListMax - 1;			

				redraw=1;
			}

			if( FPAD_OK(0) )
			{
				switch( PosX )
				{
					case 0:
					{
						ncfg->Config ^= NIN_CFG_CHEATS;
					} break;
					case 1:
					{
						ncfg->Config ^= NIN_CFG_FORCE_PROG;
					} break;
					case 2:
					{
						ncfg->Config ^= NIN_CFG_FORCE_WIDE;
					} break;
					case 3:
					{
						ncfg->Config ^= NIN_CFG_MEMCARDEMU;
					} break;
					case 4:
					{
						ncfg->Config ^= NIN_CFG_DEBUGGER;
					} break;
					case 5:
					{
						ncfg->Config ^= NIN_CFG_DEBUGWAIT;
					} break;
					case 6:
					{
						ncfg->Config ^= NIN_CFG_HID;
					} break;
					case 7:
					{
						ncfg->Config ^= NIN_CFG_OSREPORT;
					} break;
					case 8:
					{
						ncfg->Config ^= NIN_CFG_AUTO_BOOT;
					} break;
					case 9:
					{
						ncfg->MaxPads++;
						if ((ncfg->MaxPads > NIN_CFG_MAXPAD) || (ncfg->MaxPads < 1))
							ncfg->MaxPads = 1;
					} break;
					case 10:
					{
						ncfg->Language++;
						if (ncfg->Language > NIN_LAN_DUTCH)
							ncfg->Language = NIN_LAN_AUTO;
					} break;
					case 11:
					{
						ncfg->Config ^= NIN_CFG_LED;
					} break;
					case 12:
					{
						switch( ncfg->VideoMode & NIN_VID_MASK )
						{
							case NIN_VID_AUTO:
								ncfg->VideoMode &= ~NIN_VID_MASK;
								ncfg->VideoMode |= NIN_VID_FORCE;

								ListMax = 14;
							break;
							case NIN_VID_FORCE:
								ncfg->VideoMode &= ~NIN_VID_MASK;
								ncfg->VideoMode |= NIN_VID_NONE;

								ListMax = 13;

								PrintFormat( MENU_POS_X+50, 164+16*13, "                             " );

							break;
							case NIN_VID_NONE:
								ncfg->VideoMode &= ~NIN_VID_MASK;
								ncfg->VideoMode |= NIN_VID_AUTO;

								ListMax = 13;

								PrintFormat( MENU_POS_X+50, 164+16*13, "                             " );

							break;
						}

					} break;
					case 13:
					{
						switch( ncfg->VideoMode & NIN_VID_FORCE_MASK )
						{
							case NIN_VID_FORCE_PAL50:
								ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
								ncfg->VideoMode |= NIN_VID_FORCE_PAL60;
							break;
							case NIN_VID_FORCE_PAL60:
								ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
								ncfg->VideoMode |= NIN_VID_FORCE_NTSC;
							break;
							case NIN_VID_FORCE_NTSC:
								ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
								ncfg->VideoMode |= NIN_VID_FORCE_MPAL;
							break;
							case NIN_VID_FORCE_MPAL:
								ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
								ncfg->VideoMode |= NIN_VID_FORCE_PAL50;
							break;
						}

					} break;
				}
			}

			if( redraw )
			{
				PrintFormat( MENU_POS_X+50, 164+16*0, "Cheats            :%s", (ncfg->Config&NIN_CFG_CHEATS)		? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*1, "Force Progressive :%s", (ncfg->Config&NIN_CFG_FORCE_PROG)	? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*2, "Force Widescreen  :%s", (ncfg->Config&NIN_CFG_FORCE_WIDE)	? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*3, "Memcard Emulation :%s", (ncfg->Config&NIN_CFG_MEMCARDEMU)	? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*4, "Debugger          :%s", (ncfg->Config&NIN_CFG_DEBUGGER)	? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*5, "Debugger Wait     :%s", (ncfg->Config&NIN_CFG_DEBUGWAIT)	? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*6, "Use HID device    :%s", (ncfg->Config&NIN_CFG_HID)		? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*7, "OSReport          :%s", (ncfg->Config&NIN_CFG_OSREPORT)	? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*8, "Auto Boot         :%s", (ncfg->Config&NIN_CFG_AUTO_BOOT)	? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, 164+16*9, "MaxPads           :%d", (ncfg->MaxPads));

				switch( ncfg->Language )
				{
					case NIN_LAN_ENGLISH:
						PrintFormat( MENU_POS_X+50, 164+16*10,"Language          :%s", "Eng " );
					break;
					case NIN_LAN_GERMAN:
						PrintFormat( MENU_POS_X+50, 164+16*10,"Language          :%s", "Ger " );
					break;
					case NIN_LAN_FRENCH:
						PrintFormat( MENU_POS_X+50, 164+16*10,"Language          :%s", "Fre " );
					break;
					case NIN_LAN_SPANISH:
						PrintFormat( MENU_POS_X+50, 164+16*10,"Language          :%s", "Spa " );
					break;
					case NIN_LAN_ITALIAN:
						PrintFormat( MENU_POS_X+50, 164+16*10,"Language          :%s", "Ita " );
					break;
					case NIN_LAN_DUTCH:
						PrintFormat( MENU_POS_X+50, 164+16*10,"Language          :%s", "Dut " );
					break;
					default:
						ncfg->Language = NIN_LAN_AUTO;
						// no break - fall through to Auto
					case NIN_LAN_AUTO:
						PrintFormat( MENU_POS_X+50, 164+16*10,"Language          :%s", "Auto" );
					break;			
				}

				PrintFormat( MENU_POS_X+50, 164+16*11, "Drive Read LED    :%s", (ncfg->Config&NIN_CFG_LED)		? "On " : "Off" );

				switch( ncfg->VideoMode & NIN_VID_MASK )
				{
					case NIN_VID_AUTO:
						PrintFormat( MENU_POS_X+50, 164+16*12,"Video             :%s", "Auto " );
					break;
					case NIN_VID_FORCE:
						PrintFormat( MENU_POS_X+50, 164+16*12,"Video             :%s", "Force" );
					break;
					case NIN_VID_NONE:
						PrintFormat( MENU_POS_X+50, 164+16*12,"Video             :%s", "None " );
					break;		
					default:
						ncfg->VideoMode &= ~NIN_VID_MASK;
						ncfg->VideoMode |= NIN_VID_AUTO;
					break;			
				}

				if( (ncfg->VideoMode & NIN_VID_FORCE) == NIN_VID_FORCE )
				switch( ncfg->VideoMode & NIN_VID_FORCE_MASK )
				{
					case NIN_VID_FORCE_PAL50:
						PrintFormat( MENU_POS_X+50, 164+16*13, "Videomode         :%s", "PAL50" );
					break;
					case NIN_VID_FORCE_PAL60:
						PrintFormat( MENU_POS_X+50, 164+16*13, "Videomode         :%s", "PAL60" );
					break;
					case NIN_VID_FORCE_NTSC:
						PrintFormat( MENU_POS_X+50, 164+16*13, "Videomode         :%s", "NTSC " );
					break;
					case NIN_VID_FORCE_MPAL:
						PrintFormat( MENU_POS_X+50, 164+16*13, "Videomode         :%s", "MPAL " );
					break;
					default:
						ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
						ncfg->VideoMode |= NIN_VID_FORCE_NTSC;
					break;
				}

				PrintFormat( MENU_POS_X+30, 164+16*PosX, ">" );
			}
		}
		
		VIDEO_WaitVSync();
	}
	u32 SelectedGame = PosX + ScrollX;
	char* StartChar = gi[SelectedGame].Path + 3;
	if (StartChar[0] == ':')
		StartChar++;
	memcpy(ncfg->GamePath, StartChar, strlen(gi[SelectedGame].Path));
	memcpy(&(ncfg->GameID), gi[SelectedGame].ID, 4);
	DCFlushRange((void*)ncfg, sizeof(NIN_CFG));

	if (SaveSettings)
	{
		FILE *cfg;
		char ConfigPath[20];
		// Todo: detects the boot device to prevent writing twice on the same one
		sprintf(ConfigPath, "/nincfg.bin"); // writes config to boot device, loaded on next launch
		cfg = fopen(ConfigPath, "wb");
		if( cfg != NULL )
		{
			fwrite( ncfg, sizeof(NIN_CFG), 1, cfg );
			fclose( cfg );
		}
		sprintf(ConfigPath, "%s:/nincfg.bin", GetRootDevice()); // writes config to game device, used by kernel
		cfg = fopen(ConfigPath, "wb");
		if( cfg != NULL )
		{
			fwrite( ncfg, sizeof(NIN_CFG), 1, cfg );
			fclose( cfg );
		}
	}

	for( i=0; i < gamecount; ++i )
	{
		free(gi[i].Name);
		free(gi[i].Path);
	}
}

void PrintInfo()
{
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*1, "Nintendont Loader v%d.%d (%s)", NIN_VERSION>>16, NIN_VERSION&0xFFFF, IsWiiU() ? "Wii U" : "Wii");
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*2, "Built   : %s %s", __DATE__, __TIME__ );
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*3, "Firmware: %d.%d.%d", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );
}
