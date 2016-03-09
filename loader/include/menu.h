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
#ifndef __MENU_H__
#define __MENU_H__

#include <gctypes.h>

#define MAX_GAMES 500

typedef struct GameInfo 
{
	char ID[6];		// ID6 of the game.
	uint8_t NameAlloc;	// If non-zero, Name was allocated via strdup().
	                        // Otherwise, it should NOT be free()'d!
	uint8_t DiscNumber;	// Disc number.
	char *Name;		// Game name. (If NameAlloc, strdup()'d.)
	char *Path;		// File path.
} gameinfo;

void HandleSTMEvent(u32 event);
void HandleWiiMoteEvent(s32 chan);

/**
 * Select the source device and game.
 * @return TRUE to save settings; FALSE if no settings have been changed.
 */
bool SelectDevAndGame(void);

/**
 * Show the "Loading, please wait..." screen.
 * */
void ShowLoadingScreen(void);

void PrintInfo( void );
void ReconfigVideo( GXRModeObj *vidmode );
#endif
