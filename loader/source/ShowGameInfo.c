/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U
"Game Info" screen.

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

#include "ShowGameInfo.h"
#include "global.h"
#include "md5.h"
#include "FPad.h"
#include "font.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>

#include "ff_utf8.h"

extern char launch_dir[MAXPATHLEN];

// Dark gray for grayed-out menu items.
#define DARK_GRAY 0x666666FF

// String length macros.
// Used for center alignment.
#define STR_X(len) ((640 - ((len)*10)) / 2)
#define STR_CONST_X(str) STR_X(sizeof(str)-1)
#define STR_PTR_X(str) STR_X(strlen(str))

/**
 * Show the basic game information.
 * @Param gi Game to display.
 */
static void DrawGameInfoScreen(const gameinfo *gi)
{
	/**
	 * Example display:
	 *
	 * usb:/games/GALE01.gcm
	 *
	 * Title:  Super Smash Bros. Melee
	 * Region: USA
	 * Format: 1:1 full dump
	 */

	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*4, "%s", gi->Path);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*6, "Title:   %s", gi->Name);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*7, "Game ID: %.6s", gi->ID);

	static const char *const BI2regions[4] = {"Japan", "USA", "PAL", "South Korea"};
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*8, "Region:  %s",
		BI2regions[(gi->Flags & GIFLAG_REGION_MASK) >> 3]);

	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*9, "Disc #:  %u", gi->DiscNumber+1);

	static const char *const formats[8] = {
		"1:1 full dump",
		"Shrunken via DiscEx or GCToolbox",
		"Extracted FST",
		"Compressed ISO (Hermes uLoader format)",
		"Multi-Game Disc",
		"Unknown (5)",
		"Unknown (6)",
		"Unknown (7)",
	};
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*11, "Format:  %s",
		formats[gi->Flags & GIFLAG_FORMAT_MASK]);

	// TODO: MD5 verifier.

	// TODO: There's a request to determine the DSP.
	// Maybe also show banner information? Both of these
	// require extracting data from the file system.

	static const char PressHome[] = "Press HOME (or START) to return to the game list.";
	PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(PressHome), MENU_POS_Y + 20*19, PressHome);
}

/**
 * Show a game's information.
 * @param gameinfo Game to verify.
 */
void ShowGameInfo(const gameinfo *gi)
{
	// Show the basic game information.
	DrawGameInfoScreen(gi);

	// Print info with current button actions.
	PrintInfo();
	PrintButtonActions("Go Back", NULL, NULL, NULL);
	// If the selected game is 1:1, allow MD5 verification.
	const bool can_verify_md5 = ((gi->Flags & GIFLAG_FORMAT_MASK) == GIFLAG_FORMAT_FULL);
	const u32 color = (can_verify_md5 ? BLACK : DARK_GRAY);
	PrintFormat(DEFAULT_SIZE, color, MENU_POS_X + 430, MENU_POS_Y + 20*3, "X/1 : Verify MD5");

	// Render the text.
	GRRLIB_Render();
	ClearScreen();

	// Wait for the user to press the Home button.
	while (1)
	{
		VIDEO_WaitVSync();
		FPAD_Update();

		if (FPAD_Start(0))
			break;
	}
}
