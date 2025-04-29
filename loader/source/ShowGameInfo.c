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

#define MD5_BUF_SZ (1024*1024)

// MD5 verification state.
typedef struct _MD5VerifyState_t {
	bool supported;		// True if MD5 verification is supported.
	bool running;		// True if MD5 calculation is currently in progress.
	bool cancelled;		// True if MD5 calculation was cancelled.
	bool calculated;	// True if the MD5 has been calculated.
	uint8_t db_status;	// MD5_DB_Status

	// Image variables.
	FIL f_gcm;		// Disc image.
	bool gcm_read_error;	// True if a read error occurred while calculating.
	FSIZE_t image_size;	// File size.
	FSIZE_t image_read;	// Amount of the image that has been read.

	// MD5 database.
	MD5_DB_t md5_db;

	// MD5 database entry. (allocated)
	struct {
		char *id6;
		char *revision;
		char *discnum;
		char *title;
	} db_entry;

	// MD5 state.
	md5_state_t state;
	md5_byte_t digest[16];
	double time_start;
	double time_end;
	double time_diff;

	// MD5 read buffer.
	u8 *buf;

	// Text version of MD5.
	char md5_str[33];
} MD5VerifyState_t;

// ProcessDiscImage return value.
typedef enum {
	PDI_ERROR	= -1,	// Read error. (Disc image is automatically closed.)
	PDI_FINISHED	= 0,	// Finished reading. (Disc image is automatically closed.)
	PDI_NOT_DONE	= 1,	// Not finished.
	PDI_NOT_DONE_MB	= 2,	// Not finished. (MB display needs to be updated)
} PDI_RESULT;

/**
 * Get the time of day with correct microseconds.
 * @param tv struct timeval
 * @param tz struct timezone
 */
static int gettimeofday_rvlfix(struct timeval *tv, struct timezone *tz)
{
	int ret = gettimeofday(tv, tz);
	if (ret != 0)
		return ret;

	// devkitPPC's gettimeofday() is completely broken.
	// tv_sec is correct, but tv_usec always returns a value less than 400.
	// Reference: http://devkitpro.org/viewtopic.php?t=3056&p=15322
#ifdef tick_microsecs
#undef tick_microsecs
#endif
#ifdef tick_nanosecs
#undef tick_nanosecs
#endif
#define tick_microsecs(ticks) ((((u64)(ticks)*8)/(u64)(TB_TIMER_CLOCK/125))%1000000)
#define tick_nanosecs(ticks) ((((u64)(ticks)*8000)/(u64)(TB_TIMER_CLOCK/125))%1000000000)
	tv->tv_usec = tick_microsecs(gettick());
	return ret;
}

/**
 * Convert MD5 bytes to a string.
 * @param md5_str Output string. (Must be 33 bytes.)
 * @param digest MD5 digest. (Must be 16 bytes.)
 */
static void md5_to_str(char md5_str[33], const md5_byte_t digest[16])
{
	// Convert the MD5 to a string.
	static const char hex_lookup[16] = {
		'0','1','2','3','4','5','6','7',
		'8','9','a','b','c','d','e','f'
	};
	int i;
	for (i = 0; i < 16; i++) {
		md5_str[(i*2)+0] = hex_lookup[digest[i] >> 4];
		md5_str[(i*2)+1] = hex_lookup[digest[i] & 0x0F];
	}
	md5_str[32] = 0;
}

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
	 * Title:    Super Smash Bros. Melee
	 * Game ID:  GALE01
	 * Region:   USA
	 * Disc #:   1
	 * Revision: 00
	 * Format:   1:1 full dump
	 */
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*4,  "%s", gi->Path);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*6,  "Title:    %s", gi->Name);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*7,  "Game ID:  %.6s", gi->ID);

	static const char *const BI2regions[4] = {"Japan", "USA", "PAL", "South Korea"};
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*8,  "Region:   %s",
		BI2regions[(gi->Flags & GIFLAG_REGION_MASK) >> 3]);

	const u8 discNumber = ((gi->Flags & GIFLAG_DISCNUMBER_MASK) >> 5);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*9,  "Revision: %02u", gi->Revision);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*10, "Disc #:   %u", discNumber+1);

	static const char *const formats[8] = {
		"1:1 full dump",
		"Shrunken via DiscEx or GCToolbox",
		"Extracted FST",
		"Compressed ISO (Hermes uLoader format)",
		"Multi-Game Disc",
		"Oversized",
		"Unknown (6)",
		"Unknown (7)",
	};
	const u8 disc_format = (gi->Flags & GIFLAG_FORMAT_MASK);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*11, "Format:");
	PrintFormat(DEFAULT_SIZE, DiscFormatColors[disc_format],
		MENU_POS_X+(10*10), MENU_POS_Y + 20*11, "%s", formats[disc_format]);

	// Is this a 1:1 disc image?
	if (md5->supported) {
		// Print the MD5 status.
		if (md5->calculated) {
			// MD5 has been calculated.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13,
				"MD5: %.32s (%0.1fs)", md5->md5_str, md5->time_diff);
			if (md5->db_status == MD5_DB_OK) {
				if (md5->db_entry.id6 && md5->db_entry.revision && md5->db_entry.title) {
					PrintFormat(DEFAULT_SIZE, DiscFormatColors[2], MENU_POS_X, MENU_POS_Y + 20*14,
						"*** Verified: %s", md5->db_entry.title);
					PrintFormat(DEFAULT_SIZE, DiscFormatColors[2], MENU_POS_X, MENU_POS_Y + 20*15,
						"*** Game ID:  %s", md5->db_entry.id6);
					PrintFormat(DEFAULT_SIZE, DiscFormatColors[2], MENU_POS_X, MENU_POS_Y + 20*16,
						"*** Revision: %s", md5->db_entry.revision);
					if (md5->db_entry.discnum) {
						PrintFormat(DEFAULT_SIZE, DiscFormatColors[2], MENU_POS_X, MENU_POS_Y + 20*17,
							"*** Disc #:   %s", md5->db_entry.discnum);
					}
				} else {
					PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*14,
						"!!! MD5 not found in database. !!!");
				}
			}
		} else if (md5->running) {
			// MD5 calculation is in progress.
			// Show the data read so far.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13,
				"MD5: Calculating... (%u of %u MiB processed)",
				(unsigned int)(md5->image_read / (1024*1024)),
				(unsigned int)(md5->image_size / (1024*1024)));
		} else if (md5->gcm_read_error) {
			// MD5 has not been calculated due to a read error.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13, "MD5: ");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+(5*10), MENU_POS_Y + 20*13,
				"Read Error occurred (press A to try again)");
		} else if (md5->cancelled) {
			// MD5 calculation was cancelled.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13,
				"MD5: Cancelled (press A to recalculate)");
		} else {
			// MD5 has not been calculated.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13,
				"MD5: Not calculated (press A to calculate)");
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
					errmsg = "Read Error occurred";
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

	// Button actions.
	const char *const btn_B = (md5->running ? "Cancel MD5" : NULL);

	// Print information.
	PrintInfo();
	PrintButtonActions("Go Back", NULL, btn_B, NULL);

	// "Calculate MD5" should be displayed for all formats,
	// but grayed out if it isn't supported.
	// (Also grayed out if the MD5 is being calculated or if
	// it has been calculated.)
	const u32 color = ((md5->supported && (!md5->running && !md5->calculated)) ? BLACK : DARK_GRAY);
	PrintFormat(DEFAULT_SIZE, color, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : Verify MD5");
}

/**
 * Open the disc image for MD5 verification.
 * @param gi Game to open.
 * @param md5 MD5VerifyState_t
 */
static void OpenDiscImage(const gameinfo *gi, MD5VerifyState_t *md5)
{
	FIL *const f_gcm = &md5->f_gcm;

	// Open the disc image.
	if (f_open_char(f_gcm, gi->Path, FA_READ|FA_OPEN_EXISTING) != FR_OK)
	{
		// Could not open the disc image.
		md5->gcm_read_error = true;
		return;
	}

#ifdef _USE_FASTSEEK
	// Set up a FatFS link map for faster operation.
	u32 tblsize = 4; //minimum default size
	f_gcm->cltbl = malloc(tblsize * sizeof(DWORD));
	f_gcm->cltbl[0] = tblsize;
	int ret = f_lseek(&md5->f_gcm, CREATE_LINKMAP);
	if (ret == FR_NOT_ENOUGH_CORE) {
		// Need to allocate more memory for the link map.
		tblsize = f_gcm->cltbl[0];
		free(f_gcm->cltbl);
		f_gcm->cltbl = malloc(tblsize * sizeof(DWORD));
		f_gcm->cltbl[0] = tblsize;
		ret = f_lseek(&md5->f_gcm, CREATE_LINKMAP);
		if (ret != FR_OK)
		{
			// Error creating the link map.
			// We'll continue without it.
			free(f_gcm->cltbl);
			f_gcm->cltbl = NULL;
		}
	} else if (ret != FR_OK) {
		// Error creating the link map.
		// We'll continue without it.
		free(f_gcm->cltbl);
		f_gcm->cltbl = NULL;
	}
#endif /* _USE_FASTSEEK */

	// Allocate a read buffer.
	md5->buf = (u8*)memalign(32, MD5_BUF_SZ);
	if (!md5->buf) {
		// Read buffer could not be allocated.
		f_close(f_gcm);
#ifdef _USE_FASTSEEK
		free(f_gcm->cltbl);
#endif /* _USE_FASTSEEK */
		// TODO: More specific error?
		md5->gcm_read_error = true;
		return;
	}

	// Image size.
	md5->image_size = (u32)f_size(f_gcm);
	md5->image_read = 0;

	// Start time.
	struct timeval tv;
	gettimeofday_rvlfix(&tv, NULL);
	md5->time_start = tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);

	// File is open.
	md5_init(&md5->state);
	md5->running = true;
	md5->cancelled = false;
}

/**
 * Close the disc image.
 * @param md5 MD5VerifyState_t
 */
static void CloseDiscImage(MD5VerifyState_t *md5)
{
	f_close(&md5->f_gcm);
#ifdef _USE_FASTSEEK
	free(md5->f_gcm.cltbl);
#endif
	free(md5->buf);
	md5->buf = NULL;
}

/**
 * Process a megabyte of the disc image for MD5 verification.
 * @param gi Game to verify.
 * @param md5 MD5VerifyState_t
 * @return PDI_RESULT.
 */
static PDI_RESULT ProcessDiscImage(const gameinfo *gi, MD5VerifyState_t *md5)
{
	if (!md5->running)
		return PDI_FINISHED;

	FIL *const f_gcm = &md5->f_gcm;

	// Read from the disc image.
	UINT read;
	FRESULT res = f_read(f_gcm, md5->buf, MD5_BUF_SZ, &read);
	if (res != FR_OK) {
		// Read error.
		CloseDiscImage(md5);
		md5->running = false;
		md5->gcm_read_error = true;
		return PDI_ERROR;
	}

	const u32 old_mb = md5->image_read / (1024*1024);
	if (read > 0) {
		// Process the data.
		md5_append(&md5->state, (const md5_byte_t*)md5->buf, read);
		md5->image_read += read;
	}
	const u32 new_mb = md5->image_read / (1024*1024);

	if (f_eof(f_gcm)) {
		// End of file.
		CloseDiscImage(md5);
		md5->running = false;
		md5->calculated = true;

		// Finish the MD5 calculations.
		md5_finish(&md5->state, md5->digest);

		// Convert the MD5 to a string.
		md5_to_str(md5->md5_str, md5->digest);

		// Look up the MD5 in the database.
		char *db_line = NULL;
		if (md5->db_status == MD5_DB_OK) {
			db_line = FindMD5(&md5->md5_db, md5->md5_str);
		}
		if (db_line) {
			// Tokenize the line.

			// Field 1: MD5 (lowercase ASCII)
			// This field isn't actually being printed.
			char *saveptr;
			char *token = strtok_r(db_line, "|", &saveptr);

			// Field 2: ID6
			token = strtok_r(NULL, "|", &saveptr);
			md5->db_entry.id6 = (token ? strdup(token) : NULL);

			// Field 3: Revision
			token = strtok_r(NULL, "|", &saveptr);
			md5->db_entry.revision = (token ? strdup(token) : NULL);

			// Field 4: Disc number.
			// The header has the disc number, but it doesn't
			// distinguish between "Disc 1" and "single-disc game".
			token = strtok_r(NULL, "|", &saveptr);
			if (token && strcmp(token, "0") != 0) {
				md5->db_entry.discnum = strdup(token);
			} else {
				md5->db_entry.discnum = NULL;
			}

			// Field 5: Game name.
			token = strtok_r(NULL, "", &saveptr);
			md5->db_entry.title = (token ? strdup(token) : NULL);

			free(db_line);
		}

		// End time.
		struct timeval tv;
		gettimeofday_rvlfix(&tv, NULL);
		md5->time_end = tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
		md5->time_diff = md5->time_end - md5->time_start;
		return PDI_FINISHED;
	}

	// More data to process.
	return (old_mb == new_mb ? PDI_NOT_DONE : PDI_NOT_DONE_MB);
}

/**
 * Show a game's information.
 * @param gameinfo Game to display.
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

	// Wait for the user to press the Home button.
	bool redraw = true;
	while (1) {
		if (redraw) {
			// Show the game information.
			DrawGameInfoScreen(gi, &md5);
			// Render the text.
			GRRLIB_Render();
			ClearScreen();
			redraw = false;
		}

		if (md5.running) {
			// Process a megabyte of the MD5.
			PDI_RESULT res = ProcessDiscImage(gi, &md5);
			if (res != PDI_NOT_DONE) {
				// Redraw required.
				redraw = true;
				// NOTE: Don't wait for VSync here,
				// because that slows down the
				// MD5 process significantly.
				// (315.0s -> 355.7s)
			}
		} else {
			// Wait for VBlank.
			VIDEO_WaitVSync();
		}

		FPAD_Update();
		if (FPAD_Start(0)) {
			// If MD5 is running, cancel it.
			if (md5.running) {
				md5.running = false;
				md5.cancelled = true;
				CloseDiscImage(&md5);
			}
			// We're done here.
			break;
		} else if (FPAD_OK(0)) {
			// If MD5 calculation is supported, and the
			// MD5 calculation is either not in progress
			// or has not been run, start it.
			if (md5.supported && !md5.running && !md5.calculated) {
				// Start the MD5 calculation.
				OpenDiscImage(gi, &md5);
				redraw = true;
			}
		} else if (FPAD_Cancel(0)) {
			// If MD5 is running, cancel it.
			if (md5.running) {
				md5.running = false;
				md5.cancelled = true;
				CloseDiscImage(&md5);
			}
		}
	}

	if ((gi->Flags & GIFLAG_FORMAT_MASK) == GIFLAG_FORMAT_FULL) {
		FreeMD5Database(&md5.md5_db);
	}

	// Free game entry information.
	free(md5.db_entry.id6);
	free(md5.db_entry.revision);
	free(md5.db_entry.discnum);
	free(md5.db_entry.title);
}
