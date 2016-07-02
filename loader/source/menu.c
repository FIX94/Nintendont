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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ogc/stm.h>
#include <ogc/video.h>
#include <ogc/video_types.h>
#include <ogc/consol.h>
#include <ogc/system.h>
//#include <fat.h>
#include <di/di.h>
#include "menu.h"
#include "../../common/include/CommonConfigStrings.h"

#include "ff_utf8.h"

// Device state.
typedef enum {
	// Device is open and has a "games" directory.
	DEV_OK = 0,
	// Device could not be opened.
	DEV_NO_OPEN = 1,
	// Device was opened but has no "games" directory.
	DEV_NO_GAMES = 2,
} DevState;
static u8 devState = DEV_OK;

/**
 * Print information about the selected device.
 */
static void PrintDevInfo(void);

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

	int ret = strcasecmp(da->Name, db->Name);
	if (ret == 0)
	{
		// Names are equal. Check disc number.
		if (da->DiscNumber < db->DiscNumber)
			ret = -1;
		else if (da->DiscNumber > db->DiscNumber)
			ret = 1;
		else
			ret = 0;
	}
	return ret;
}

/**
 * Get all games from the games/ directory on the selected storage device.
 * On Wii, this also adds a pseudo-game for loading GameCube games from disc.
 *
 * @param gi           [out] Array of gameinfo structs.
 * @param sz           [in]  Maximum number of elements in gi.
 * @param pGameCount   [out] Number of games loaded. (Includes GCN pseudo-game for Wii.)
 *
 * @return DevState value:
 * - DEV_OK: Device opened and has a "games/" directory.
 * - DEV_NO_OPEN: Could not open the storage device.
 * - DEV_NO_GAMES: No "games/" directory was found.
 */
static DevState LoadGameList(gameinfo *gi, u32 sz, u32 *pGameCount)
{
	// Create a list of games
	char filename[MAXPATHLEN];	// Current filename.
	char gamename[65];		// Game title.
	char buf[0x100];		// Disc header.
	int gamecount = 0;		// Current game count.

	if( !IsWiiU() )
	{
		// Pseudo game for booting a GameCube disc on Wii.
		gi[0].ID[0] = 'D',gi[0].ID[1] = 'I',gi[0].ID[2] = 'S';
		gi[0].ID[3] = 'C',gi[0].ID[4] = '0',gi[0].ID[5] = '1';
		gi[0].Name = "Boot GC Disc in Drive";
		gi[0].NameAlloc = 0;
		gi[0].DiscNumber = 0;
		gi[0].Path = strdup("di:di");
		gamecount++;
	}

	DIR pdir;
	snprintf(filename, sizeof(filename), "%s:/games", GetRootDevice());
	if (f_opendir_char(&pdir, filename) != FR_OK)
	{
		// Could not open the "games" directory.

		// Attempt to open the device root.
		snprintf(filename, sizeof(filename), "%s:/", GetRootDevice());
		if (f_opendir_char(&pdir, filename) != FR_OK)
		{
			// Could not open the device root.
			if (pGameCount)
				*pGameCount = 0;
			return DEV_NO_OPEN;
		}

		// Device root opened.
		// This means the device is usable, but it
		// doesn't have a "games" directory.
		f_closedir(&pdir);
		if (pGameCount)
			*pGameCount = gamecount;
		return DEV_NO_GAMES;
	}

	// Process the directory.
	// TODO: chdir into /games/?
	FILINFO fInfo;
	FIL in;
	while (f_readdir(&pdir, &fInfo) == FR_OK && fInfo.fname[0] != '\0')
	{
		// Game layout should be: /games/GAMEID/game.iso
		// Search for subdirectories.
		if (fInfo.fattrib & AM_DIR)
		{
			// Skip "." and "..".
			// This will also skip "hidden" directories.
			if (fInfo.fname[0] == '.')
				continue;

			// Prepare the filename buffer with the directory name.
			// game.iso/disc2.iso will be appended later.
			// NOTE: fInfo.fname[] is UTF-16.
			const char *filename_utf8 = wchar_to_char(fInfo.fname);
			int fnlen = snprintf(filename, sizeof(filename), "%s:/games/%s/",
					     GetRootDevice(), filename_utf8);

			//Test if game.iso exists and add to list
			bool found = false;
			u32 DiscNumber;
			for (DiscNumber = 0; DiscNumber < 2; DiscNumber++)
			{
				if (DiscNumber)
					memcpy(&filename[fnlen], "disc2.iso", 10);
				else
					memcpy(&filename[fnlen], "game.iso", 9);

				if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) == FR_OK)
				{
					// Read the disc header
					//gprintf("(%s) ok\n", filename );
					UINT read;
					f_read(&in, buf, 0x100, &read);
					f_close(&in);

					if (read == 0x100 && IsGCGame((u8*)buf))	// Must be GC game
					{
						memcpy(gi[gamecount].ID, buf, 6); //ID for EXI
						gi[gamecount].DiscNumber = DiscNumber;

						// Check if this title is in titles.txt.
						const char *dbTitle = SearchTitles(gi[gamecount].ID);
						if (dbTitle)
						{
							// Title found.
							gi[gamecount].Name = (char*)dbTitle;
							gi[gamecount].NameAlloc = 0;
						}
						else
						{
							// Title not found.
							// Use the title from the disc header.
							strncpy(gamename, buf + 0x20, sizeof(gamename)-1);
							gamename[sizeof(gamename)-1] = 0;
							gi[gamecount].Name = strdup(gamename);
							gi[gamecount].NameAlloc = 1;
						}

						gi[gamecount].Path = strdup( filename );
						gamecount++;
						found = true;
					}
				}
			}

			// If game.iso wasn't found, check for FST format.
			if (!found)
			{
				memcpy(&filename[fnlen], "sys/boot.bin", 13);
				if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) == FR_OK)
				{
					//gprintf("(%s) ok\n", filename );
					UINT read;
					f_read(&in, buf, 0x100, &read);
					f_close(&in);

					if (read == 0x100 && IsGCGame((u8*)buf))	// Must be GC game
					{
						// Terminate the filename at the game's base directory.
						filename[fnlen] = 0;

						memcpy(gi[gamecount].ID, buf, 6); //ID for EXI
						gi[gamecount].DiscNumber = DiscNumber;

						// TODO: Check titles.txt?
						strncpy(gamename, buf + 0x20, sizeof(gamename)-1);
						gamename[sizeof(gamename)-1] = 0;
						gi[gamecount].Name = strdup(gamename);
						gi[gamecount].NameAlloc = 1;

						gi[gamecount].Path = strdup( filename );
						gamecount++;
					}
				}
			}
		}

		if (gamecount >= sz)	//if array is full
			break;
	}
	f_closedir(&pdir);

	// Sort the list alphabetically.
	// On Wii, the pseudo-entry for GameCube discs is always
	// kept at the top.
	if( IsWiiU() )
		qsort(gi, gamecount, sizeof(gameinfo), compare_names);
	else if( gamecount > 1 )
		qsort(&gi[1], gamecount-1, sizeof(gameinfo), compare_names);

	// Save the game count.
	if (pGameCount)
		*pGameCount = gamecount;

	return DEV_OK;
}

/**
 * Select a game from the specified device.
 * @return Bitfield indicating the user's selection:
 * - 0 == go back
 * - 1 == game selected
 * - 2 == go back and save settings (UNUSED)
 * - 3 == game selected and save settings
 */
static int SelectGame(void)
{
	// Depending on how many games are on the storage device,
	// this could take a while.
	ShowLoadingScreen();

	// Load the game list.
	u32 gamecount = 0;
	gameinfo gi[MAX_GAMES];

	devState = LoadGameList(&gi[0], MAX_GAMES, &gamecount);
	switch (devState)
	{
		case DEV_OK:
			// Game list loaded successfully.
			break;

		case DEV_NO_GAMES:
			// No "games" directory was found.
			// The list will still be shown, since there's a
			// "Boot GC Disc in Drive" option on Wii.
			gprintf("WARNING: %s:/games/ was not found.\n", GetRootDevice());
			break;

		case DEV_NO_OPEN:
		default:
		{
			// Could not open the device at all.
			// The list won't be shown, since a storage device
			// is required for various functionality, but the
			// user will be able to go back to the previous menu.
			const char *s_devType = (UseSD ? "SD" : "USB");
			gprintf("No %s FAT device found.\n", s_devType);
			break;
		}
	}

	bool selected = false;	// Set to TRUE if the user selected a game.

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

	// set default game to game that currently set in configuration
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
			// Go back to the Settings menu.
			selected = false;
			break;
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
				// User selected a game.
				selected = true;
				break;
			}

			if( redraw )
			{
				PrintInfo();
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*0, "Home: Go Back");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : %s", MenuMode ? "Modify" : "Select");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*2, "B   : %s", MenuMode ? "Game List" : "Settings ");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*3, MenuMode ? "X/1 : Update" : "");
				PrintDevInfo();

				// Starting position.
				int gamelist_y = MENU_POS_Y + 20*4;
				if (devState != DEV_OK)
				{
					// The warning message overlaps "Boot GC Disc in Drive".
					// Move the list down by one row.
					gamelist_y += 20;
				}

				for (i = 0; i < ListMax; ++i, gamelist_y += 20)
				{
					// FIXME: Print all 64 characters of the game name?
					// Currently truncated to 50.
					const gameinfo *cur_gi = &gi[i+ScrollX];
					if (cur_gi->DiscNumber == 0)
					{
						// Disc 1.
						PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, gamelist_y,
							    "%50.50s [%.6s]%s",
							    cur_gi->Name, cur_gi->ID,
							    i == PosX ? ARROW_LEFT : " ");
					}
					else
					{
						// Disc 2 or higher.
						PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, gamelist_y,
							    "%46.46s (%d) [%.6s]%s",
							    cur_gi->Name, cur_gi->DiscNumber+1, cur_gi->ID,
							    i == PosX ? ARROW_LEFT : " ");
					}
				}
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
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*0, "Home: Go Back");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : %s", MenuMode ? "Modify" : "Select");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*2, "B   : %s", MenuMode ? "Game List" : "Settings ");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*3, MenuMode ? "X/1 : Update" : "");
				if (devState == DEV_OK)
				{
					// FIXME: If devState != DEV_OK,
					// the device info overlaps with the settings menu.
					PrintDevInfo();
				}
				GRRLIB_Render();
				Screenshot();
				ClearScreen();
				redraw = 0;
			}
		}
	}

	u32 SelectedGame = PosX + ScrollX;
	char* StartChar = gi[SelectedGame].Path + 3;
	if (StartChar[0] == ':')
		StartChar++;
	memcpy(ncfg->GamePath, StartChar, strlen(gi[SelectedGame].Path));
	memcpy(&(ncfg->GameID), gi[SelectedGame].ID, 4);
	DCFlushRange((void*)ncfg, sizeof(NIN_CFG));

	// Free allocated memory in the game list.
	for (i = 0; i < gamecount; ++i)
	{
		if (gi[i].NameAlloc)
			free(gi[i].Name);
		free(gi[i].Path);
	}

	if (!selected)
	{
		// No game selected.
		return 0;
	}

	// Game is selected.
	// TODO: Return an enum.
	return (SaveSettings ? 3 : 1);
}

/**
 * Select the source device and game.
 * @return TRUE to save settings; FALSE if no settings have been changed.
 */
bool SelectDevAndGame(void)
{
	// Select the source device. (SD or USB)
	bool SaveSettings = false;
	while (1)
	{
		VIDEO_WaitVSync();
		FPAD_Update();

		UseSD = (ncfg->Config & NIN_CFG_USB) == 0;
		PrintInfo();
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*0, "Home: Exit");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : Select");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 6, UseSD ? ARROW_LEFT : "");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 7, UseSD ? "" : ARROW_LEFT);
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 6, " SD  ");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 7, "USB  ");

		// Render the screen here to prevent a blank frame
		// when returning from SelectGame().
		GRRLIB_Render();
		ClearScreen();

		if (FPAD_OK(0))
		{
			// Select a game from the specified device.
			int ret = SelectGame();
			if (ret & 2) SaveSettings = true;
			if (ret & 1) break;
		}
		else if (FPAD_Start(0))
		{
			ShowMessageScreenAndExit("Returning to loader...", 0);
		}
		else if (FPAD_Down(0))
		{
			ncfg->Config = ncfg->Config | NIN_CFG_USB;
		}
		else if (FPAD_Up(0))
		{
			ncfg->Config = ncfg->Config & ~NIN_CFG_USB;
		}
	}

	return SaveSettings;
}

/**
 * Show a single message screen.
 * @param msg Message.
 */
void ShowMessageScreen(const char *msg)
{
	const int len = strlen(msg);
	const int x = (640 - (len*10)) / 2;

	ClearScreen();
	PrintInfo();
	PrintFormat(DEFAULT_SIZE, BLACK, x, 232, "%s", msg);
	GRRLIB_Render();
	ClearScreen();
}

/**
 * Show a single message screen and then exit to loader..
 * @param msg Message.
 * @param ret Return value. If non-zero, text will be printed in red.
 */
void ShowMessageScreenAndExit(const char *msg, int ret)
{
	const int len = strlen(msg);
	const int x = (640 - (len*10)) / 2;
	const u32 color = (ret == 0 ? BLACK : MAROON);

	ClearScreen();
	PrintInfo();
	PrintFormat(DEFAULT_SIZE, color, x, 232, "%s", msg);
	ExitToLoader(ret);
}

/**
 * Print Nintendont version and system hardware information.
 */
void PrintInfo(void)
{
#ifdef NIN_SPECIAL_VERSION
	// "Special" version with customizations. (Not mainline!)
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*0, "Nintendont Loader v%u.%u" NIN_SPECIAL_VERSION " (%s)",
		    NIN_VERSION>>16, NIN_VERSION&0xFFFF, IsWiiU() ? "Wii U" : "Wii");
#else
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*0, "Nintendont Loader v%u.%u (%s)",
		    NIN_VERSION>>16, NIN_VERSION&0xFFFF, IsWiiU() ? "Wii U" : "Wii");
#endif
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*1, "Built   : " __DATE__ " " __TIME__);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*2, "Firmware: %u.%u.%u",
		    *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143);
}

/**
 * Print information about the selected device.
 */
static void PrintDevInfo(void)
{
	// Device type.
	const char *s_devType = (UseSD ? "SD" : "USB");

	// Device state.
	// NOTE: If this is showing a message, the game list
	// will be moved down by 1 row, which usually isn't
	// a problem, since it will either be empty or showing
	// "Boot GC Disc in Drive".
	switch (devState) {
		case DEV_NO_OPEN:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
				"WARNING: %s FAT device could not be opened.", s_devType);
			break;
		case DEV_NO_GAMES:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
				"WARNING: %s:/games/ was not found.", GetRootDevice());
			break;
		default:
			break;
	}
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

/**
 * Print a LoadKernel() error message.
 *
 * This function does NOT force a return to loader;
 * that must be handled by the caller.
 * Caller must also call UpdateScreen().
 *
 * @param iosErr IOS loading error ID.
 * @param err Return value from the IOS function.
 */
void PrintLoadKernelError(LoadKernelError_t iosErr, s32 err)
{
	ClearScreen();
	PrintInfo();
	PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4, "Failed to load IOS58 from NAND:");

	switch (iosErr)
	{
		case LKERR_UNKNOWN:
		default:
			// TODO: Add descriptions of more LoadKernel() errors.
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "LoadKernel() error %d occurred, returning %d.", iosErr, err);
			break;

		case LKERR_ES_GetStoredTMDSize:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "ES_GetStoredTMDSize() returned %d.", err);
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "This usually means IOS58 is not installed.");
			if (IsWiiU())
			{
				// No IOS58 on Wii U should never happen...
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*9, "WARNING: On Wii U, a missing IOS58 may indicate");
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "something is seriously wrong with the vWii setup.");
			}
			else
			{
				// TODO: Check if we're using System 4.3.
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*9, "Please update to Wii System 4.3 and try running");
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "Nintendont again.");
			}
			break;

		case LKERR_TMD_malloc:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to allocate memory for the IOS58 TMD.");
			break;

		case LKERR_ES_GetStoredTMD:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "ES_GetStoredTMD() returned %d.", err);
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
			break;

		case LKERR_IOS_Open_shared1_content_map:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Open(\"/shared1/content.map\") returned %d.", err);
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "This usually means Nintendont was not started with");
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*8, "AHB access permissions.");
			// FIXME: Create meta.xml if it isn't there?
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "Please ensure that meta.xml is present in your Nintendont");
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*11, "application directory and that it contains a line");
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*12, "with the tag <ahb_access/> .");
			break;

		case LKERR_IOS_Open_IOS58_kernel:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Open(IOS58 kernel) returned %d.", err);
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
			break;

		case LKERR_IOS_Read_IOS58_kernel:
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Read(IOS58 kernel) returned %d.", err);
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
			break;
	}
}
