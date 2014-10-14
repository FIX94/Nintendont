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
#include "update.h"
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
#include "../../common/include/CommonConfigStrings.h"

extern NIN_CFG* ncfg;

u32 Shutdown = 0;
extern char *launch_dir;

inline u32 SettingY(u32 row)
{
	return 148 + 16 * row;
}
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
int compare_names(const void *a, const void *b)
{
	const gameinfo *da = (const gameinfo *) a;
	const gameinfo *db = (const gameinfo *) b;

	return strcasecmp(da->Name, db->Name);
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
						memcpy(gi[gamecount].ID, buf, 6); //ID for EXI
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

						memcpy(gi[gamecount].ID, buf, 6); //ID for EXI
						gi[gamecount].Name = strdup( buf + 0x20 );
						gi[gamecount].Path = strdup( filename );

						gamecount++;
					}
				}
			}
		}
		if (gamecount >= MAX_GAMES)	//if array is full
			break;
	}

	if( gamecount == 0 )
	{
		ClearScreen();
		gprintf("No games found in %s:/games !\n", GetRootDevice());
		PrintFormat( 25, 232, "No games found in %s:/games !", GetRootDevice() );
		ExitToLoader(1);
	}
	qsort(gi, gamecount, sizeof(gameinfo), compare_names);

	u32 redraw = 1;
	u32 i;
	s32 PosX = 0, prevPosX = 0;
	s32 ScrollX = 0, prevScrollX = 0;
	u32 MenuMode = 0;

	u32 ListMax = gamecount;
	if( ListMax > 14 )
		ListMax = 14;
	bool SaveSettings = false;

//	set default game to game that currently set in configuration
	for (i = 0; i < gamecount; ++i)
	{
		if (strcasecmp(strchr(gi[i].Path,':')+1, ncfg->GamePath) == 0)
		{
			if( i >= ListMax )
			{
				PosX	= ListMax - 1;
				ScrollX = i - ListMax + 1;
			} else {
				PosX = i;
			}
			break;
		}
	}

	while(1)
	{
		PrintInfo();
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*1, "Home: Exit");
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*2, "A   : %s", MenuMode ? "Modify" : "Select");
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*3, "B   : %s", MenuMode ? "Game List" : "Settings ");
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*4, "%s", MenuMode ? "X   : Update" : "            ");

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

			if( MenuMode == 0 )
			{
				ListMax = gamecount;
				if( ListMax > 14 )
					ListMax = 14;
				PosX = prevPosX;
				ScrollX = prevScrollX;
			}
			else
			{
				ListMax = NIN_SETTINGS_LAST;

				prevPosX = PosX;
				PosX	= 0;
				prevScrollX = ScrollX;
				ScrollX = 0;
			}

			redraw = 1;

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
				SaveSettings = true;
			}
			else if( FPAD_Right(0) )
			{
				PrintFormat( MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

				if( PosX == ListMax - 1 )
				{
					if( PosX + ListMax + ScrollX < gamecount)
						ScrollX = ScrollX + ListMax;
					else
					if( PosX + ScrollX != gamecount -1)
						ScrollX = gamecount - ListMax;
					else {
						PosX	= 0;
						ScrollX = 0;
					}
				} else {
					PosX = ListMax - 1;
				}
			
				redraw=1;
				SaveSettings = true;
			}
			else if( FPAD_Up(0) )
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
				SaveSettings = true;
			}
			else if( FPAD_Left(0) )
			{
				PrintFormat( MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

				if( PosX == 0 )
				{
					if( ScrollX - (s32)ListMax >= 0 )
						ScrollX = ScrollX - ListMax;
					else
					if( ScrollX != 0 )
						ScrollX = 0;
					else {
						ScrollX = gamecount - ListMax;
					}
				} else {
					PosX = 0;
				}

				redraw=1;
				SaveSettings = true;
			}

			if( FPAD_OK(0) )
			{
				break;
			}

			if( redraw )
			{
				for( i=0; i < ListMax; ++i )
					PrintFormat( MENU_POS_X-8, MENU_POS_Y + 20*6 + i * 20, "%42.42s [%.6s]", gi[i+ScrollX].Name, gi[i+ScrollX].ID );
					
				PrintFormat( MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, "<" );
			}

		} else {	//settings menu		
			
			if(FPAD_X(0)) {
				ClearScreen();
				UpdateNintendont(launch_dir);
				ClearScreen();
				redraw = 1;
			}
			
			if( FPAD_Down(0) )
			{
				PrintFormat( MENU_POS_X+30, SettingY(PosX), " " );
				
				PosX++;

				if (((ncfg->VideoMode & NIN_VID_FORCE) != NIN_VID_FORCE) && (PosX == NIN_SETTINGS_VIDEOMODE))
					PosX++;
				if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDBLOCKS))
					PosX++;
				if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDMULTI))
					PosX++;
				if (PosX >= ListMax)
				{
					ScrollX = 0;
					PosX	= 0;
				}
			
				redraw=1;

			} else if( FPAD_Up(0) )
			{
				PrintFormat( MENU_POS_X+30, SettingY(PosX), " " );

				PosX--;

				if (PosX < 0)
					PosX = ListMax - 1;
				if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDMULTI))
					PosX--;
				if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDBLOCKS))
					PosX--;
				if (((ncfg->VideoMode & NIN_VID_FORCE) != NIN_VID_FORCE) && (PosX == NIN_SETTINGS_VIDEOMODE))
					PosX--;

				redraw=1;
			}

			if( FPAD_OK(0) )
			{
				SaveSettings = true;
				if ( PosX < NIN_CFG_BIT_LAST )
					ncfg->Config ^= (1 << PosX);
				else switch( PosX )
				{
					case NIN_SETTINGS_MAX_PADS:
					{
						ncfg->MaxPads++;
						if (ncfg->MaxPads > NIN_CFG_MAXPAD)
							ncfg->MaxPads = 0;
					} break;
					case NIN_SETTINGS_LANGUAGE:
					{
						ncfg->Language++;
						if (ncfg->Language > NIN_LAN_DUTCH)
							ncfg->Language = NIN_LAN_AUTO;
					} break;
					case NIN_SETTINGS_VIDEO:
					{
						u32 Video = (ncfg->VideoMode & NIN_VID_MASK) >> 16;
						Video = (Video + 1) % 3;
						Video <<= 16;
						ncfg->VideoMode &= ~NIN_VID_MASK;
						ncfg->VideoMode |= Video;
						if (Video != NIN_VID_FORCE)
							PrintFormat( MENU_POS_X+50, SettingY(NIN_SETTINGS_VIDEOMODE), "%29s", "" );
					} break;
					case NIN_SETTINGS_VIDEOMODE:
					{
						u32 Video = (ncfg->VideoMode & NIN_VID_FORCE_MASK);
						Video = Video << 1;
						if (Video > NIN_VID_FORCE_MPAL)
							Video = NIN_VID_FORCE_PAL50;
						ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
						ncfg->VideoMode |= Video;
					} break;
					case NIN_SETTINGS_MEMCARDBLOCKS:
					{
						ncfg->MemCardBlocks++;
						if (ncfg->MemCardBlocks > MEM_CARD_MAX)
							ncfg->MemCardBlocks = 0;
					} break;
					case NIN_SETTINGS_MEMCARDMULTI:
					{
						ncfg->Config ^= (NIN_CFG_MC_MULTI);
					} break;
				}
				if (!(ncfg->Config & NIN_CFG_MEMCARDEMU))
				{
					PrintFormat(MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDBLOCKS), "%29s", "");
					PrintFormat(MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDMULTI), "%29s", "");
				}
			}

			if( redraw )
			{
				u32 ListLoopIndex = 0;
				for (ListLoopIndex = 0; ListLoopIndex < NIN_CFG_BIT_LAST; ListLoopIndex++)
					PrintFormat( MENU_POS_X+50, SettingY(ListLoopIndex), "%-18s:%s", OptionStrings[ListLoopIndex], (ncfg->Config & (1 << ListLoopIndex)) ? "On " : "Off" );
				PrintFormat( MENU_POS_X+50, SettingY(ListLoopIndex), "%-18s:%d", OptionStrings[ListLoopIndex], (ncfg->MaxPads));
				ListLoopIndex++;

				u32 LanIndex = ncfg->Language;
				if (( LanIndex >= NIN_LAN_LAST ) ||  ( LanIndex < NIN_LAN_FIRST ))
					LanIndex = NIN_LAN_LAST; //Auto
				PrintFormat( MENU_POS_X+50, SettingY(ListLoopIndex),"%-18s:%-4s", OptionStrings[ListLoopIndex], LanguageStrings[LanIndex] );
				ListLoopIndex++;

				u32 VideoIndex = (ncfg->VideoMode & NIN_VID_MASK) >> 16;
				if (( VideoIndex > NIN_VID_INDEX_NONE ) ||  ( VideoIndex < NIN_VID_INDEX_AUTO ))
				{
					ncfg->VideoMode &= ~NIN_VID_MASK;
					ncfg->VideoMode |= NIN_VID_AUTO;
					VideoIndex = NIN_VID_INDEX_AUTO; //Auto
				}
				PrintFormat( MENU_POS_X+50, SettingY(ListLoopIndex),"%-18s:%-5s", OptionStrings[ListLoopIndex], VideoStrings[VideoIndex] );
				ListLoopIndex++;

				if( (ncfg->VideoMode & NIN_VID_FORCE) == NIN_VID_FORCE )
				{
					u32 VideoModeVal = ncfg->VideoMode & NIN_VID_FORCE_MASK;
					u32 VideoModeIndex;
					u32 VideoTestVal = 1;
					for (VideoModeIndex = 0; (VideoTestVal <= NIN_VID_FORCE_MPAL) && (VideoModeVal != VideoTestVal); VideoModeIndex++)
						VideoTestVal <<= 1;
					if ( VideoModeVal < VideoTestVal )
					{
						ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
						ncfg->VideoMode |= NIN_VID_FORCE_NTSC;
						VideoModeIndex = NIN_VID_INDEX_FORCE_NTSC;
					}
					PrintFormat( MENU_POS_X+50, SettingY(ListLoopIndex),"%-18s:%-5s", OptionStrings[ListLoopIndex], VideoModeStrings[VideoModeIndex] );
				}
				ListLoopIndex++;

				if ((ncfg->Config & NIN_CFG_MEMCARDEMU) == NIN_CFG_MEMCARDEMU)
				{
					u32 MemCardBlocksVal = ncfg->MemCardBlocks;
					if (MemCardBlocksVal > MEM_CARD_MAX)
						MemCardBlocksVal = 0;
					PrintFormat(MENU_POS_X + 50, SettingY(ListLoopIndex), "%-18s:%-4d", OptionStrings[ListLoopIndex], MEM_CARD_BLOCKS(MemCardBlocksVal));
					PrintFormat(MENU_POS_X + 50, SettingY(ListLoopIndex+1), "%-18s:%-4s", OptionStrings[ListLoopIndex+1], (ncfg->Config & (NIN_CFG_MC_MULTI)) ? "On " : "Off");
				}
				ListLoopIndex+=2;

				PrintFormat(MENU_POS_X + 30, SettingY(PosX), ">");
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
