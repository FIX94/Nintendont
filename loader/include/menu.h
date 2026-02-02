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
#include <ogc/video.h>

#define MAX_GAMES 1024

typedef enum
{
	// Disc format.
	GIFLAG_FORMAT_FULL	= (0 << 0),	// 1:1 rip
	GIFLAG_FORMAT_SHRUNKEN	= (1 << 0),	// Shrunken via DiscEX
	GIFLAG_FORMAT_FST	= (2 << 0),	// Extracted FST
	GIFLAG_FORMAT_CISO	= (3 << 0),	// CISO format
	GIFLAG_FORMAT_MULTI	= (4 << 0),	// Multi-game disc
	GIFLAG_FORMAT_OVER	= (5 << 0),	// Oversized
	GIFLAG_FORMAT_MASK	= (7 << 0),

	// Game region. (from bi2.bin)
	// BI2region_codes values, lshifted by 2.
	GIFLAG_REGION_MASK	= (3 << 3),

	// Disc number, lshifted by 5.
	GIFLAG_DISCNUMBER_MASK	= (3 << 5),

	// GameInfo.Name was allocated via strdup()
	// and must be freed.
	GIFLAG_NAME_ALLOC	= (1 << 7),
} GameInfoFlags;

typedef struct GameInfo 
{
	// NOTE: Disc number is stored in Flags.
	// There aren't any games that take up more
	// than 2 discs, so we're using two bits in
	// Flags for the disc number now.
	char ID[6];		// ID6 of the game.
	uint8_t Revision;	// Disc revision.
	uint8_t Flags;		// See GameInfoFlags.

	char *Name;		// Game name. (If NameAlloc, strdup()'d.)
	char *Path;		// File path.
} gameinfo;

// Disc format colors.
extern const u32 DiscFormatColors[8];

void SetShutdown(void);
void HandleSTMEvent(u32 event);
void HandleWiiMoteEvent(s32 chan);

/**
 * Select the source device and game.
 * @return TRUE to save settings; FALSE if no settings have been changed.
 */
bool SelectDevAndGame(void);

/**
 * Show a single message screen.
 * @param msg Message.
 */
void ShowMessageScreen(const char *msg);

/**
 * Show a single message screen and then exit to loader..
 * @param msg Message.
 * @param ret Return value. If non-zero, text will be printed in red.
 */
void ShowMessageScreenAndExit(const char *msg, int ret)
	__attribute__ ((noreturn));

// Predefined messages.
static inline void ShowLoadingScreen(void)
{
	ShowMessageScreen("Loading, please wait...");
}

/**
 * Print Nintendont version and system hardware information.
 */
void PrintInfo(void);

/**
 * Print button actions.
 * Call this function after PrintInfo().
 *
 * If any button action is NULL, that button won't be displayed.
 *
 * @param btn_home	[in,opt] Home button action.
 * @param btn_a		[in,opt] A button action.
 * @param btn_b		[in,opt] B button action.
 * @param btn_x1	[in,opt] X/1 button action.
 */
void PrintButtonActions(const char *btn_home, const char *btn_a, const char *btn_b, const char *btn_x1);

typedef enum {
	LKERR_UNKNOWN,
	LKERR_ES_GetStoredTMDSize,
	LKERR_TMD_malloc,
	LKERR_ES_GetStoredTMD,
	LKERR_IOS_Open_shared1_content_map,
	LKERR_HashNotFound,
	LKERR_IOS_Open_IOS58_kernel,
	LKERR_IOS_Read_IOS58_kernel,
	
} LoadKernelError_t;

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
void PrintLoadKernelError(LoadKernelError_t iosErr, int err);

void ReconfigVideo( GXRModeObj *vidmode );
#endif
