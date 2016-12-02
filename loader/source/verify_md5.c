/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U
MD5sum verifier.

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

#include "verify_md5.h"
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

/**
 * MD5 database format:
 * 3a62f8d10fd210d4928ad37e3816e33c|GALE01|00|0|Super Smash Bros. Melee
 * - Field 1: MD5 (lowercase ASCII)
 * - Field 2: ID6
 * - Field 3: Revision (two-digit decimal)
 * - Field 4: Disc number (0 for single-disc; 1 or 2 for multi)
 * - Field 5: Game name
 */

typedef enum {
	MD5_DB_OK = 0,
	MD5_DB_NOT_FOUND,
	MD5_DB_TOO_BIG,
	MD5_DB_NO_MEM,
	MD5_DB_READ_ERROR,
} MD5_DB_STATUS;

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

#define STR_X(len) ((640 - ((len)*10)) / 2)
#define STR_CONST_X(str) STR_X(sizeof(str)-1)
#define STR_PTR_X(str) STR_X(strlen(str))

/**
 * Show the MD5 database status.
 * Call this function after one of the regular Show*() functions.
 * @param md5_db_status MD5 database status.
 */
static void ShowMD5DBStatus(MD5_DB_STATUS md5_db_status)
{
	switch (md5_db_status)
	{
		case MD5_DB_OK:
			// MD5 database has been loaded.
			break;

		case MD5_DB_NOT_FOUND: {
			// Could not find the MD5 database.
			static const char db_not_found[] = "WARNING: gcn_md5.txt not found; cannot verify MD5s.";
			PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(db_not_found), 232+180, db_not_found);
			break;
		}

		case MD5_DB_TOO_BIG: {
			// MD5 database is too big.
			static const char db_too_big[] = "WARNING: gcn_md5.txt is larger than 1 MiB; cannot load.";
			PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(db_too_big), 232+180, db_too_big);
			break;
		}

		case MD5_DB_NO_MEM: {
			// Could not allocate memory for the MD5 database.
			static const char db_no_mem[] = "WARNING: memalign() failed loading the MD5 database.";
			PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(db_no_mem), 232+180, db_no_mem);
			break;
		}

		case MD5_DB_READ_ERROR:
		default: {
			// Error reading the MD5 database.
			static const char db_read_error[] = "WARNING: Read error loading MD5 database.";
			PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(db_read_error), 232+180, db_read_error);
			break;
		}
	}
}

/**
 * Show a status update.
 * NOTE: Caller should print other lines and then render the text.
 * @param gamepath Game path.
 * @param total_read Total number of bytes read so far.
 * @param total_size Total size of the disc image.
 */
static void ShowStatusUpdate(const char *gamepath, u32 total_read, u32 total_size)
{
	// Status update.
	ClearScreen();
	PrintInfo();
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*2, "B   : Cancel");

	static const char md5_calculating[] = "Calculating MD5...";
	PrintFormat(DEFAULT_SIZE, BLACK, STR_CONST_X(md5_calculating), 232-40, md5_calculating);
	// TODO: Abbreviate the path if it's too long.
	PrintFormat(DEFAULT_SIZE, BLACK, STR_PTR_X(gamepath), 232, "%s", gamepath);

	// Print the status, in MiB.
	char status_msg[128];
	int len = snprintf(status_msg, sizeof(status_msg),
		"%u of %u MiB processed",
		total_read / (1024*1024), total_size / (1024*1024));
	PrintFormat(DEFAULT_SIZE, BLACK, STR_X(len), 232+40, "%s", status_msg);

	// Caller should print other lines and then render the text.
}
/**
 * Show the "Finish" screen.
 * NOTE: Caller should print other lines and then render the text.
 * @param time_diff Time taken to calculate the MD5.
 * @param gamepath Game path.
 * @param md5_str MD5 as a string.
 */
static void ShowFinishScreen(double time_diff, const char *gamepath, const char *md5_str)
{
	char status_msg[128];

	ClearScreen();
	PrintInfo();
	int len = snprintf(status_msg, sizeof(status_msg), "MD5 calculated in %0.1f seconds.", time_diff);
	PrintFormat(DEFAULT_SIZE, BLACK, STR_X(len), 232-40, status_msg);
	// TODO: Abbreviate the path if it's too long.
	PrintFormat(DEFAULT_SIZE, BLACK, STR_PTR_X(gamepath), 232, "%s", gamepath);

	// Print the MD5.
	len = snprintf(status_msg, sizeof(status_msg), "MD5: %s", md5_str);
	PrintFormat(DEFAULT_SIZE, BLACK, STR_X(len), 232+40, "%s", status_msg);

	// Caller should print other lines and then render the text.
}

/**
 * Find an MD5 in the MD5 database.
 * @param md5_db	[in] MD5 database in memory.
 * @param md5_db_size	[in] Size of md5_db, in bytes.
 * @param md5_str	[in] MD5 string in lowercase ASCII. (32 chars + NULL)
 * @return Copy of the MD5 line from the database, NULL-terminated; NULL if not found. (Must be freed after use!)
 */
static char *FindMD5(const char *md5_db, u32 md5_db_size, const char *md5_str)
{
	// End of MD5 DB. (actually one past, same semantics as C++ iterators)
	const char *md5_db_end = md5_db + md5_db_size;

	while (md5_db < md5_db_end)
	{
		// Find the newline and/or NULL terminator.
		const char *nl;
		for (nl = md5_db; nl < md5_db_end; nl++) {
			if (*nl == '\n' || *nl == 0)
				break;
		}

		// Found the newline and/or NULL terminator.
		const u32 str_sz = (u32)(nl - md5_db);
		if (str_sz < 33) {
			// Empty line, or line isn't big enough.
			if (nl == md5_db_end || *nl == 0) {
				// End of database or NULL terminator.
				break;
			}
			// Next line.
			md5_db = nl + 1;
			continue;
		}

		// Compare the first 32 characters to the requested MD5.
		// NOTE: This will handle comment lines, since # isn't
		// a valid character in an MD5.
		if (md5_db[32] == '|' && !memcmp(md5_db, md5_str, 32)) {
			// We have a match!
			char *buf = (char*)malloc(str_sz+1);
			if (!buf)
				return NULL;
			memcpy(buf, md5_db, str_sz);
			buf[str_sz] = 0;
			return buf;
		}

		// Not a match. Proceed to the next line.
		md5_db = nl + 1;
	}

	// No match.
	return NULL;
}

/**
 * Print a line from the MD5 database.
 * @param md5_db_line MD5 database line. (May be modified by strtok_r().)
 * @param header Minimal disc header for the game ID in case of conflicts.
 */
static void PrintMD5Line(char *md5_db_line, const u8 header[16])
{
	char *saveptr;
	// strdup()'d tokens.
	char *discnum = NULL;

	// Field 1: MD5 (lowercase ASCII)
	// This field isn't actually being printed.
	char *token = strtok_r(md5_db_line, "|", &saveptr);
	if (!token) {
		// No MD5. (Shouldn't happen...)
		static const char no_md5_field[] = "!!! MD5 DB error -1 !!!";
		PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(no_md5_field), 232+60, no_md5_field);
		return;
	}

	// Field 2: ID6
	// NOTE: Ignored in favor of 'header'.
	token = strtok_r(NULL, "|", &saveptr);

	// Field 3: Revision
	// NOTE: Ignored in favor of 'header'.
	token = strtok_r(NULL, "|", &saveptr);

	// Field 4: Disc number.
	// The header has the disc number, but it doesn't
	// distinguish between "Disc 1" and "single-disc game".
	token = strtok_r(NULL, "|", &saveptr);
	if (token)
		discnum = strdup(token);

	// Field 5: Game name.
	token = strtok_r(NULL, "", &saveptr);

	// Print everything.
#define FST_GREEN 0x00551AFF	/* Same color used for FST format on the game list. */
	static const char md5_found[] = "MD5 verified against GameTDB's database.";
	PrintFormat(DEFAULT_SIZE, FST_GREEN, STR_CONST_X(md5_found), 232+60, md5_found);
	if (token)
	{
		PrintFormat(DEFAULT_SIZE, BLACK, STR_PTR_X(token), 232+80, token);
	}
	else
	{
		static const char no_game_name[] = "(no game name)";
		PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(no_game_name), 232+80, no_game_name);
	}

	// ID6, revision, disc number.
	// ID6 and revision are obtained from the header, since some
	// GameTDB entries have the same MD5 for different ID6s.
	// Disc number is taken from the MD5 database file.
	char buf[40];
	if (discnum && discnum[0] != '0')
	{
		snprintf(buf, sizeof(buf), "%.6s, Rev.%02u, Disc %.1s", header, header[7], discnum);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%.6s, Rev.%02u", header, header[7]);
	}
	PrintFormat(DEFAULT_SIZE, BLACK, STR_PTR_X(buf), 232+100, buf);

	free(discnum);
}

/**
 * Verify a game's MD5.
 * @param gameinfo Game to verify.
 */
void VerifyMD5(const gameinfo *gi)
{
	md5_state_t state;
	md5_byte_t digest[16];
	md5_init(&state);

	ClearScreen();
	PrintInfo();

	// Initial status.
	static const char opening_str[] = "Opening disc image for verification:";
	PrintFormat(DEFAULT_SIZE, BLACK, STR_CONST_X(opening_str), 232-40, opening_str);
	PrintFormat(DEFAULT_SIZE, BLACK, STR_PTR_X(gi->Path), 232, "%s", gi->Path);
	GRRLIB_Render();
	ClearScreen();

	// Open the file.
	FIL in;
	if (f_open_char(&in, gi->Path, FA_READ|FA_OPEN_EXISTING) != FR_OK)
	{
		// Could not open the disc image.
		// TODO: Show an error.
		return;
	}

#ifdef _USE_FASTSEEK
	// Set up a FatFS link map for faster operation.
	u32 tblsize = 4; //minimum default size
	in.cltbl = malloc(tblsize * sizeof(DWORD));
	in.cltbl[0] = tblsize;
	int ret = f_lseek(&in, CREATE_LINKMAP);
	if (ret == FR_NOT_ENOUGH_CORE)
	{
		// Need to allocate more memory for the link map.
		tblsize = in.cltbl[0];
		free(in.cltbl);
		in.cltbl = malloc(tblsize * sizeof(DWORD));
		in.cltbl[0] = tblsize;
		ret = f_lseek(&in, CREATE_LINKMAP);
		if (ret != FR_OK)
		{
			// Error creating the link map.
			// We'll continue without it.
			free(in.cltbl);
			in.cltbl = NULL;
		}
	} else if (ret != FR_OK)
	{
		// Error creating the link map.
		// We'll continue without it.
		free(in.cltbl);
		in.cltbl = NULL;
	}
#endif /* _USE_FASTSEEK */

	// Determine the gcn_md5.txt path.
	// If loaded from network, launch_dir[] is empty,
	// so use /apps/Nintendont/ as a fallback.
	char filepath[MAXPATHLEN];
	snprintf(filepath, sizeof(filepath), "%sgcn_md5.txt",
		 launch_dir[0] != 0 ? launch_dir : "/apps/Nintendont/");
	FIL f_md5;
	MD5_DB_STATUS md5_db_status = MD5_DB_OK;
	char *md5_db = NULL;
	u32 md5_db_size = 0;
	FRESULT res = f_open_char(&f_md5, filepath, FA_READ|FA_OPEN_EXISTING);
	if (res == FR_OK)
	{
		// Load the MD5 database. (Should be 1 MB or less.)
		static const u32 md5_db_size_max = 1024*1024;
		if (f_size(&f_md5) <= md5_db_size_max)
		{
			md5_db_size = (u32)f_size(&f_md5);
			md5_db = (char*)memalign(32, md5_db_size);
			if (md5_db)
			{
				UINT read;
				FRESULT res = f_read(&f_md5, md5_db, md5_db_size, &read);
				f_close(&f_md5);
				if (res != FR_OK || read != md5_db_size)
				{
					// Read error.
					md5_db_status = MD5_DB_READ_ERROR;
					md5_db_size = 0;
					free(md5_db);
					md5_db = NULL;
				}
			}
			else
			{
				// Memory allocation failed.
				md5_db_status = MD5_DB_NO_MEM;
				md5_db_size = 0;
			}
		}
		else
		{
			// MD5 database is too big.
			md5_db_status = MD5_DB_TOO_BIG;
		}
		f_close(&f_md5);
	}
	else if (res == FR_NO_FILE || res == FR_NO_PATH)
	{
		// File not found.
		md5_db_status = MD5_DB_NOT_FOUND;
	}
	else
	{
		// Some other error...
		// TODO: Better error code?
		md5_db_status = MD5_DB_READ_ERROR;
	}

	// Read 1 MB at a time.
	static const u32 buf_sz = 1024*1024;
	u8 *buf = (u8*)memalign(32, buf_sz);
	if (!buf)
	{
		// Memory allocation failed.
		// TODO: Show an error.
		f_close(&in);
#ifdef _USE_FASTSEEK
		free(in.cltbl);
#endif
		free(md5_db);
		return;
	}

	// Minimal disc header.
	// Used to show the actual game ID in case there's
	// conflicts with multiple versions.
	u8 header[16];

	// Start time.
	struct timeval tv;
	gettimeofday_rvlfix(&tv, NULL);
	const double time_start = tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);

	u32 total_read = 0;
	u32 total_read_mb = 0;
	const u32 total_size = f_size(&in);

	// Show the initial status update screen.
	ShowStatusUpdate(gi->Path, total_read, total_size);
	ShowMD5DBStatus(md5_db_status);
	// Render the text.
	GRRLIB_Render();
	ClearScreen();

	bool cancel = false;
	while (!f_eof(&in))
	{
		FPAD_Update();
		if (FPAD_Cancel(0))
		{
			// User cancelled the operation.
			cancel = true;
			break;
		}

		UINT read;
		FRESULT res = f_read(&in, buf, buf_sz, &read);
		if (res != FR_OK)
		{
			// TODO: Show an error.
			cancel = true;
			break;
		}

		if (total_read == 0) {
			// Save the minimal disc header.
			memcpy(header, buf, sizeof(header));
		}

		// Process the data.
		md5_append(&state, (const md5_byte_t*)buf, read);
		total_read += read;

		// Only update the screen if the total read MB has changed.
		u32 new_total_read_mb = total_read / (1024*1024);
		if (new_total_read_mb != total_read_mb)
		{
			new_total_read_mb = total_read_mb;
			ShowStatusUpdate(gi->Path, total_read, total_size);
			ShowMD5DBStatus(md5_db_status);
			// Render the text.
			GRRLIB_Render();
			ClearScreen();
		}
	}

	free(buf);
	f_close(&in);
#ifdef _USE_FASTSEEK
	free(in.cltbl);
#endif

	if (cancel)
	{
		// User cancelled the operation.
		free(md5_db);
		return;
	}

	// Finish the MD5 calculations.
	md5_finish(&state, digest);

	// End time.
	gettimeofday_rvlfix(&tv, NULL);
	const double time_end = tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
	const double time_diff = time_end - time_start;

	// Finished processing the MD5.
	char md5_str[33];
	md5_to_str(md5_str, digest);
	ShowFinishScreen(time_diff, gi->Path, md5_str);
	ShowMD5DBStatus(md5_db_status);

	if (md5_db != NULL)
	{
		// Check the MD5 database.
		static const char checking_db[] = "Checking MD5 database...";
		PrintFormat(DEFAULT_SIZE, BLACK, STR_CONST_X(checking_db), 232+60, checking_db);
		// Render the text.
		GRRLIB_Render();
		ClearScreen();

		// Look up the MD5 in the database.
		char *md5_db_line = FindMD5(md5_db, md5_db_size, md5_str);

		// Do we have a match?
		ShowFinishScreen(time_diff, gi->Path, md5_str);
		ShowMD5DBStatus(md5_db_status);
		if (md5_db_line)
		{
			// Found a match!
			PrintMD5Line(md5_db_line, header);
			free(md5_db_line);
		}
		else
		{
			// No match.
			static const char no_md5_match_1[] = "!!! MD5 not found in database. !!!";
			static const char no_md5_match_2[] = "!!!   This may be a bad dump.  !!!";
			PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(no_md5_match_1), 232+60, no_md5_match_1);
			PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(no_md5_match_2), 232+80, no_md5_match_2);
		}
	}
	else
	{
		static const char cannot_verify[] = "Cannot verify MD5 due to database error.";
		PrintFormat(DEFAULT_SIZE, MAROON, STR_CONST_X(cannot_verify), 232+80, cannot_verify);
	}

	static const char press_a_button[] = "Press the A button to continue...";
	PrintFormat(DEFAULT_SIZE, BLACK, STR_CONST_X(press_a_button), 232+140, press_a_button);
	// Render the text.
	GRRLIB_Render();
	ClearScreen();

	// Wait for the user to press the A button.
	while (1)
	{
		VIDEO_WaitVSync();
		FPAD_Update();

		if (FPAD_OK(0))
			break;
	}

	free(md5_db);
}
