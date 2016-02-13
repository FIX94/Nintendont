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
#include <gccore.h>
#include "font.h"
#include "exi.h"
#include "global.h"
#include "FPad.h"
#include "Config.h"
#include "update.h"
#include "titles.h"
#include "dip.h"
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
#include <di/di.h>
#include "menu.h"
#include "../../common/include/CommonConfigStrings.h"

extern NIN_CFG* ncfg;

u32 Shutdown = 0;
extern char *launch_dir;

inline u32 SettingY(u32 row)
{
	return 127 + 16 * row;
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
bool SelectGame( void )
{
//Create a list of games
	char filename[MAXPATHLEN];
	char gamename[MAXPATHLEN];

	DIR *pdir;
	struct dirent *pent;
	struct stat statbuf;

	snprintf(filename, sizeof(filename), "%s:/games", GetRootDevice());
	pdir = opendir(filename);
	if( !pdir )
	{
		ClearScreen();
		gprintf("No FAT device found, or missing %s dir!\n", filename);
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, 232, "No FAT device found, or missing %s dir!", filename );
		ExitToLoader(1);
	}

	u32 gamecount = 0;
	char buf[0x100];
	gameinfo gi[MAX_GAMES];

	memset( gi, 0, sizeof(gameinfo) * MAX_GAMES );
	if( !IsWiiU() )
	{
		gi[0].Name = strdup("Boot GC Disc in Drive");
		gi[0].ID[0] = 'D',gi[0].ID[1] = 'I',gi[0].ID[2] = 'S';
		gi[0].ID[3] = 'C',gi[0].ID[4] = '0',gi[0].ID[5] = '1';
		gi[0].Path = strdup("di:di");
		gamecount++;
	}
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
				snprintf(filename, sizeof(filename), "%s:/games/%s/%s.iso", GetRootDevice(), pent->d_name, DiscNumber ? "disc2" : "game");

				FILE *in = fopen( filename, "rb" );
				if( in != NULL )
				{
				//	gprintf("(%s) ok\n", filename );
					fread( buf, 1, 0x100, in );
					fclose(in);

					if( IsGCGame((u8*)buf) )	// Must be GC game
					{
						memcpy(gi[gamecount].ID, buf, 6); //ID for EXI
						if(!SearchTitles(gi[gamecount].ID, gamename)) strcpy( gamename, buf + 0x20 );
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
				snprintf(filename, sizeof(filename), "%s:/games/%s/sys/boot.bin", GetRootDevice(), pent->d_name);

				FILE *in = fopen( filename, "rb" );
				if( in != NULL )
				{
				//	gprintf("(%s) ok\n", filename );
					fread( buf, 1, 0x100, in );
					fclose(in);

					if( IsGCGame((u8*)buf) )	// Must be GC game
					{
						snprintf(filename, sizeof(filename), "%s:/games/%s/", GetRootDevice(), pent->d_name);

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
	closedir(pdir);

	if( IsWiiU() )
		qsort(gi, gamecount, sizeof(gameinfo), compare_names);
	else if( gamecount > 1 )
		qsort(&gi[1], gamecount-1, sizeof(gameinfo), compare_names);

	u32 redraw = 1;
	u32 i;
	u32 settingPart = 0;
	s32 PosX = 0, prevPosX = 0;
	s32 ScrollX = 0, prevScrollX = 0;
	u32 MenuMode = 0;

	u32 ListMax = gamecount;
	if( ListMax > 15 )
		ListMax = 15;
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

	u32 UpHeld = 0, DownHeld = 0;
	while(1)
	{
		VIDEO_WaitVSync();
		FPAD_Update();

		if( FPAD_Start(1) )
		{
			ClearScreen();
			PrintFormat(DEFAULT_SIZE, BLACK, 212, 232, "Returning to loader..." );
			ExitToLoader(0);
		}

		if( FPAD_Cancel(0) )
		{
			MenuMode ^= 1;

			if( MenuMode == 0 )
			{
				ListMax = gamecount;
				if( ListMax > 15 )
					ListMax = 15;
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
				settingPart = 0;
			}

			redraw = 1;
		}

	//	gprintf("\rS:%u P:%u G:%u M:%u    ", ScrollX, PosX, gamecount, ListMax );

		if( MenuMode == 0 )		//game select menu
		{
			if( FPAD_Down(1) )
			{
				if(DownHeld == 0 || DownHeld > 10)
				{
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

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
				
					if((ncfg->Config & NIN_CFG_CHEATS) && (ncfg->Config & NIN_CFG_CHEAT_PATH))	//if cheat path being used
						ncfg->Config = ncfg->Config & ~(NIN_CFG_CHEATS | NIN_CFG_CHEAT_PATH);		//clear it beacuse it cant be correct for a different game
					redraw=1;
					SaveSettings = true;
					}
				DownHeld++;
			}
			else
				DownHeld = 0;
			if( FPAD_Right(0) )
			{
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

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
			
				if((ncfg->Config & NIN_CFG_CHEATS) && (ncfg->Config & NIN_CFG_CHEAT_PATH))	//if cheat path being used
					ncfg->Config = ncfg->Config & ~(NIN_CFG_CHEATS | NIN_CFG_CHEAT_PATH);		//clear it beacuse it cant be correct for a different game
				redraw=1;
				SaveSettings = true;
			}
			else if( FPAD_Up(1) )
			{
				if(UpHeld == 0 || UpHeld > 10)
				{
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

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

					if((ncfg->Config & NIN_CFG_CHEATS) && (ncfg->Config & NIN_CFG_CHEAT_PATH))	//if cheat path being used
						ncfg->Config = ncfg->Config & ~(NIN_CFG_CHEATS | NIN_CFG_CHEAT_PATH);		//clear it beacuse it cant be correct for a different game
					redraw=1;
					SaveSettings = true;
				}
				UpHeld++;
			}
			else
				UpHeld = 0;
			if( FPAD_Left(0) )
			{
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + PosX * 20, " " );

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

				if((ncfg->Config & NIN_CFG_CHEATS) && (ncfg->Config & NIN_CFG_CHEAT_PATH))	//if cheat path being used
					ncfg->Config = ncfg->Config & ~(NIN_CFG_CHEATS | NIN_CFG_CHEAT_PATH);		//clear it beacuse it cant be correct for a different game
				redraw=1;
				SaveSettings = true;
			}

			if( FPAD_OK(0) )
			{
				break;
			}

			if( redraw )
			{
				PrintInfo();
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*0, "Home: Exit");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : %s", MenuMode ? "Modify" : "Select");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*2, "B   : %s", MenuMode ? "Game List" : "Settings ");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*3, MenuMode ? "X/1 : Update" : "");
				for( i=0; i < ListMax; ++i )
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*4 + i * 20, "%50.50s [%.6s]%s", gi[i+ScrollX].Name, gi[i+ScrollX].ID, i == PosX ? ARROW_LEFT : " " );
				GRRLIB_Render();
				Screenshot();
				ClearScreen();
				redraw = 0;
			}

		} else {	//settings menu
			
			if(FPAD_X(0)) {
				UpdateNintendont();
				redraw = 1;
			}

			if( FPAD_Down(0) )
			{
				if(settingPart == 0)
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+30, SettingY(PosX), " " );
				else
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+300, SettingY(PosX), " " );
				PosX++;
				if(settingPart == 0)
				{
					if (((ncfg->VideoMode & NIN_VID_FORCE) == 0) && (PosX == NIN_SETTINGS_VIDEOMODE))
						PosX++;
					if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDBLOCKS))
						PosX++;
					if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDMULTI))
						PosX++;
				}
				if ((settingPart == 0 && PosX >= ListMax)
					|| (settingPart == 1 && PosX >= 3))
				{
					ScrollX = 0;
					PosX	= 0;
					settingPart ^= 1;
				}
			
				redraw=1;

			}
			else if( FPAD_Up(0) )
			{
				if(settingPart == 0)
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+30, SettingY(PosX), " " );
				else
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+300, SettingY(PosX), " " );

				PosX--;

				if (PosX < 0)
				{
					settingPart ^= 1;
					if(settingPart == 0)
						PosX = ListMax - 1;
					else
						PosX = 2;
				}
				if(settingPart == 0)
				{
					if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDMULTI))
						PosX--;
					if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (PosX == NIN_SETTINGS_MEMCARDBLOCKS))
						PosX--;
					if (((ncfg->VideoMode & NIN_VID_FORCE) == 0) && (PosX == NIN_SETTINGS_VIDEOMODE))
						PosX--;
				}
				redraw=1;
			}

			if( FPAD_Left(0) )
			{
				if(settingPart == 1)
				{
					SaveSettings = true;
					if(PosX == 0)
					{
						if(ncfg->VideoScale == 0)
							ncfg->VideoScale = 120;
						else
						{
							ncfg->VideoScale-=2;
							if(ncfg->VideoScale < 40 || ncfg->VideoScale > 120)
								ncfg->VideoScale = 0; //auto
						}
						ReconfigVideo(rmode);
						redraw = 1;
					}
					else if(PosX == 1)
					{
						ncfg->VideoOffset--;
						if(ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20)
							ncfg->VideoOffset = 20;
						ReconfigVideo(rmode);
						redraw = 1;
					}
				}
			}
			else if( FPAD_Right(0) )
			{
				if(settingPart == 1)
				{
					SaveSettings = true;
					if(PosX == 0)
					{
						if(ncfg->VideoScale == 0)
							ncfg->VideoScale = 40;
						else
						{
							ncfg->VideoScale+=2;
							if(ncfg->VideoScale < 40 || ncfg->VideoScale > 120)
								ncfg->VideoScale = 0; //auto
						}
						ReconfigVideo(rmode);
						redraw = 1;
					}
					else if(PosX == 1)
					{
						ncfg->VideoOffset++;
						if(ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20)
							ncfg->VideoOffset = -20;
						ReconfigVideo(rmode);
						redraw = 1;
					}
				}
			}

			if( FPAD_OK(0) )
			{
				if(settingPart == 0)
				{
					SaveSettings = true;
					if ( PosX < NIN_CFG_BIT_LAST )
					{
						if(PosX == NIN_CFG_BIT_USB) //Option gets replaced
							ncfg->Config ^= NIN_CFG_WIIU_WIDE;
						else
							ncfg->Config ^= (1 << PosX);
					}
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
							u32 Video = (ncfg->VideoMode & NIN_VID_MASK);
							switch (Video)
							{
								case NIN_VID_AUTO:
									Video = NIN_VID_FORCE;
									break;
								case NIN_VID_FORCE:
									Video = NIN_VID_FORCE | NIN_VID_FORCE_DF;
									break;
								case NIN_VID_FORCE | NIN_VID_FORCE_DF:
									Video = NIN_VID_NONE;
									break;
								default:
								case NIN_VID_NONE:
									Video = NIN_VID_AUTO;
									break;
							}
							ncfg->VideoMode &= ~NIN_VID_MASK;
							ncfg->VideoMode |= Video;
							if ((Video & NIN_VID_FORCE) == 0)
								PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(NIN_SETTINGS_VIDEOMODE), "%29s", "" );
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
						case NIN_SETTINGS_NATIVE_SI:
						{
							ncfg->Config ^= (NIN_CFG_NATIVE_SI);
						} break;
					}
					if (!(ncfg->Config & NIN_CFG_MEMCARDEMU))
					{
						PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDBLOCKS), "%29s", "");
						PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDMULTI), "%29s", "");
					}
					redraw = 1;
				}
				else if(settingPart == 1)
				{
					if(PosX == 2)
					{
						SaveSettings = true;
						ncfg->VideoMode ^= (NIN_VID_PATCH_PAL50);
						redraw = 1;
					}
				}
			}

			if( redraw )
			{
				u32 ListLoopIndex = 0;
				for (ListLoopIndex = 0; ListLoopIndex < NIN_CFG_BIT_LAST; ListLoopIndex++)
				{
					if(ListLoopIndex == NIN_CFG_BIT_USB) //Option gets replaced
						PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex), "%-18s:%s", OptionStrings[ListLoopIndex], (ncfg->Config & (NIN_CFG_WIIU_WIDE)) ? "On " : "Off");
					else
						PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex), "%-18s:%s", OptionStrings[ListLoopIndex], (ncfg->Config & (1 << ListLoopIndex)) ? "On " : "Off" );
				}
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex), "%-18s:%d", OptionStrings[ListLoopIndex], (ncfg->MaxPads));
				ListLoopIndex++;

				u32 LanIndex = ncfg->Language;
				if (( LanIndex >= NIN_LAN_LAST ) ||  ( LanIndex < NIN_LAN_FIRST ))
					LanIndex = NIN_LAN_LAST; //Auto
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),"%-18s:%-4s", OptionStrings[ListLoopIndex], LanguageStrings[LanIndex] );
				ListLoopIndex++;

				u32 VideoModeIndex;
				u32 VideoModeVal = ncfg->VideoMode & NIN_VID_MASK;
				switch (VideoModeVal)
				{
					case NIN_VID_AUTO:
						VideoModeIndex = NIN_VID_INDEX_AUTO;
						break;
					case NIN_VID_FORCE:
						VideoModeIndex = NIN_VID_INDEX_FORCE;
						break;
					case NIN_VID_FORCE | NIN_VID_FORCE_DF:
						VideoModeIndex = NIN_VID_INDEX_FORCE_DF;
						break;
					case NIN_VID_NONE:
						VideoModeIndex = NIN_VID_INDEX_NONE;
						break;
					default:
						ncfg->VideoMode &= ~NIN_VID_MASK;
						ncfg->VideoMode |= NIN_VID_AUTO;
						VideoModeIndex = NIN_VID_INDEX_AUTO;
				}
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),"%-18s:%-18s", OptionStrings[ListLoopIndex], VideoStrings[VideoModeIndex] );
				ListLoopIndex++;

				if( ncfg->VideoMode & NIN_VID_FORCE )
				{
					VideoModeVal = ncfg->VideoMode & NIN_VID_FORCE_MASK;
					u32 VideoTestVal = NIN_VID_FORCE_PAL50;
					for (VideoModeIndex = 0; (VideoTestVal <= NIN_VID_FORCE_MPAL) && (VideoModeVal != VideoTestVal); VideoModeIndex++)
						VideoTestVal <<= 1;
					if ( VideoModeVal < VideoTestVal )
					{
						ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
						ncfg->VideoMode |= NIN_VID_FORCE_NTSC;
						VideoModeIndex = NIN_VID_INDEX_FORCE_NTSC;
					}
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),"%-18s:%-5s", OptionStrings[ListLoopIndex], VideoModeStrings[VideoModeIndex] );
				}
				ListLoopIndex++;

				if ((ncfg->Config & NIN_CFG_MEMCARDEMU) == NIN_CFG_MEMCARDEMU)
				{
					u32 MemCardBlocksVal = ncfg->MemCardBlocks;
					if (MemCardBlocksVal > MEM_CARD_MAX)
						MemCardBlocksVal = 0;
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex), "%-18s:%-4d%s", OptionStrings[ListLoopIndex], MEM_CARD_BLOCKS(MemCardBlocksVal), MemCardBlocksVal > 2 ? "Unstable" : "");
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex+1), "%-18s:%-4s", OptionStrings[ListLoopIndex+1], (ncfg->Config & (NIN_CFG_MC_MULTI)) ? "On " : "Off");
				}
				ListLoopIndex+=2;

				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex), "%-18s:%-4s", OptionStrings[ListLoopIndex], (ncfg->Config & (NIN_CFG_NATIVE_SI)) ? "On " : "Off");
				ListLoopIndex++;

				char vidWidth[10];
				if(ncfg->VideoScale < 40 || ncfg->VideoScale > 120)
				{
					ncfg->VideoScale = 0;
					snprintf(vidWidth, sizeof(vidWidth), "Auto");
				}	
				else
					snprintf(vidWidth, sizeof(vidWidth), "%i", ncfg->VideoScale + 600);

				char vidOffset[10];
				if(ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20)
					ncfg->VideoOffset = 0;
				snprintf(vidOffset, sizeof(vidOffset), "%i", ncfg->VideoOffset);

				ListLoopIndex = 0; //reset on other side
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex), "%-18s:%-4s", "Video Width", vidWidth);
				ListLoopIndex++;
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex), "%-18s:%-4s", "Screen Position", vidOffset);
				ListLoopIndex++;
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex), "%-18s:%-4s", "Patch PAL50", (ncfg->VideoMode & (NIN_VID_PATCH_PAL50)) ? "On " : "Off");
				ListLoopIndex++;
				if(settingPart == 0)
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 30, SettingY(PosX), ARROW_RIGHT);
				else
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 300, SettingY(PosX), ARROW_RIGHT);
				PrintInfo();
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*0, "Home: Exit");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : %s", MenuMode ? "Modify" : "Select");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*2, "B   : %s", MenuMode ? "Game List" : "Settings ");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*3, MenuMode ? "X/1 : Update" : "");
				GRRLIB_Render();
				Screenshot();
				ClearScreen();
				redraw = 0;
			}
		}
	}
	ClearScreen();
	PrintFormat(DEFAULT_SIZE, BLACK, 212, 232, "Loading, please wait...");
	GRRLIB_Render();
	ClearScreen();

	u32 SelectedGame = PosX + ScrollX;
	char* StartChar = gi[SelectedGame].Path + 3;
	if (StartChar[0] == ':')
		StartChar++;
	memcpy(ncfg->GamePath, StartChar, strlen(gi[SelectedGame].Path));
	memcpy(&(ncfg->GameID), gi[SelectedGame].ID, 4);
	DCFlushRange((void*)ncfg, sizeof(NIN_CFG));

	for( i=0; i < gamecount; ++i )
	{
		free(gi[i].Name);
		free(gi[i].Path);
	}
	return SaveSettings;
}

void PrintInfo()
{
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*0, "Nintendont Loader v%d.%d (%s)", NIN_VERSION>>16, NIN_VERSION&0xFFFF, IsWiiU() ? "Wii U" : "Wii");
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*1, "Built   : %s %s", __DATE__, __TIME__ );
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*2, "Firmware: %d.%d.%d", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );
}

void ReconfigVideo(GXRModeObj *vidmode)
{
	if(ncfg->VideoScale >= 40 && ncfg->VideoScale <= 120)
		vidmode->viWidth = ncfg->VideoScale + 600;
	else
		vidmode->viWidth = 640;
	vidmode->viXOrigin = (720 - vidmode->viWidth) / 2;

	if(ncfg->VideoOffset >= -20 && ncfg->VideoOffset <= 20)
	{
		if((vidmode->viXOrigin + ncfg->VideoOffset) < 0)
			vidmode->viXOrigin = 0;
		else if((vidmode->viXOrigin + ncfg->VideoOffset) > 80)
			vidmode->viXOrigin = 80;
		else
			vidmode->viXOrigin += ncfg->VideoOffset;
	}
	VIDEO_Configure(vidmode);
}
