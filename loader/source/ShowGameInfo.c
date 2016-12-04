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
#include "md5.h"
#include "md5_db.h"

// Dark gray for grayed-out menu items.
#define DARK_GRAY 0x666666FF

// String length macros.
// Used for center alignment.
#define STR_X(len) ((640 - ((len)*10)) / 2)
#define STR_CONST_X(str) STR_X(sizeof(str)-1)
#define STR_PTR_X(str) STR_X(strlen(str))

// MD5 verification state.
typedef struct _MD5VerifyState_t {
	bool supported;		// True if MD5 verification is supported.
	bool calculated;	// True if the MD5 has been calculated.
	uint8_t db_status;	// MD5_DB_Status

	// Image variables.
	FSIZE_t image_size;	// File size.
	FSIZE_t image_read;	// Amount of the image that has been read.

	// MD5 database.
	MD5_DB_t md5_db;

	// MD5 state.
	md5_state_t state;
	md5_byte_t digest[16];

	// Text version of MD5.
	char md5_str[33];
} MD5VerifyState_t;

/**
 * Show the basic game information.
 * @param gi Game to display.
 * @param md5 MD5VerifyState_t
 */
static void DrawGameInfoScreen(const gameinfo *gi, const MD5VerifyState_t *md5)
{
	/**
	 * Example display:
	 *
	 * usb:/games/GALE01.gcm
	 *
	 * Title:   Super Smash Bros. Melee
	 * Game ID: GALE01
	 * Region:  USA
	 * Disc #:  1
	 *
	 * Format:  1:1 full dump
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
	const u8 disc_format = (gi->Flags & GIFLAG_FORMAT_MASK);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*11, "Format:");
	PrintFormat(DEFAULT_SIZE, DiscFormatColors[disc_format],
		MENU_POS_X+(9*10), MENU_POS_Y + 20*11, "%s", formats[disc_format]);

	// Is this a 1:1 disc image?
	if (md5->supported) {
		// Print the MD5 status.
		// TODO: If calculating, show "Cancel" for B.
		if (md5->calculated) {
			// MD5 has been calculated.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13,
				"MD5: %.32s", md5->md5_str);
			// TODO: Did this match anything in the database?
		} else {
			// MD5 has not been calculated.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13,
				"MD5: Not calculated - press A to calculate");
		}

		if (md5->db_status != MD5_DB_OK) {
			// MD5 database error.
			const char *errmsg;
			switch (md5->db_status) {
				case MD5_DB_NOT_FOUND:
					// Could not find the MD5 database.
					errmsg = "gcn_md5.txt not found";
					break;
				case MD5_DB_TOO_BIG:
					// MD5 database is too big.
					errmsg = "gcn_md5.txt is larger than 1 MiB";
					break;
				case MD5_DB_NO_MEM:
					// Could not allocate memory for the MD5 database.
					errmsg = "memory allocation failed";
					break;
				case MD5_DB_READ_ERROR:
				default:
					// Error reading the MD5 database.
					errmsg = "Read error occurred";
					break;
			}

			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*17,
				"WARNING: MD5 database error: %s.", errmsg);
		}
	} else {
		// Not a 1:1 disc image.
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13,
			"MD5 verification is disabled for this game because");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14,
			"it is not a 1:1 full dump.");
	}

	// TODO: There's a request to determine the DSP.
	// Maybe also show banner information? Both of these
	// require extracting data from the file system.

	static const char PressHome[] = "Press HOME (or START) to return to the game list.";
	PrintFormat(DEFAULT_SIZE, BLACK, STR_CONST_X(PressHome), MENU_POS_Y + 20*19, PressHome);

	// Print information.
	PrintInfo();
	PrintButtonActions("Go Back", NULL, NULL, NULL);

	// "Calculate MD5" should be displayed in all cases,
	// but grayed out if it isn't supported.
	const u32 color = (md5->supported ? BLACK : DARK_GRAY);
	PrintFormat(DEFAULT_SIZE, color, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : Verify MD5");
}

/**
 * Show a game's information.
 * @param gameinfo Game to verify.
 */
void ShowGameInfo(const gameinfo *gi)
{
	MD5VerifyState_t md5;

	// Initialize the MD5 verification state.
	memset(&md5, 0, sizeof(md5));
	// If the selected game is 1:1, allow MD5 verification.
	md5.supported = ((gi->Flags & GIFLAG_FORMAT_MASK) == GIFLAG_FORMAT_FULL);
	if (md5.supported) {
		// 1:1 image. MD5 can be checked.
		// Load the database.
		md5.db_status = LoadMD5Database(&md5.md5_db);
	}

	// Show the game information.
	DrawGameInfoScreen(gi, &md5);

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

	if ((gi->Flags & GIFLAG_FORMAT_MASK) == GIFLAG_FORMAT_FULL) {
		FreeMD5Database(&md5.md5_db);
	}
}
