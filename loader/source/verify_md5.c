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
#include <sys/time.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>

#include "ff_utf8.h"

extern char launch_dir[MAXPATHLEN];

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
static void ShowFinishScreen(float time_diff, const char *gamepath, const char *md5_str)
{
	char status_msg[128];

	ClearScreen();
	PrintInfo();
	int len = snprintf(status_msg, sizeof(status_msg), "MD5 calculated in %0.1f seconds.", time_diff);
	PrintFormat(DEFAULT_SIZE, BLACK, STR_X(len), 232-40, status_msg);
	PrintFormat(DEFAULT_SIZE, BLACK, STR_PTR_X(gamepath), 232, "%s", gamepath);

	// Print the MD5.
	len = snprintf(status_msg, sizeof(status_msg), "MD5: %s", md5_str);
	PrintFormat(DEFAULT_SIZE, BLACK, STR_X(len), 232+40, "%s", status_msg);

	// Caller should print other lines and then render the text.
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
				FRESULT res = f_read(&in, md5_db, md5_db_size, &read);
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

	// Read 64 KB at a time.
	static const u32 buf_sz = 1024*1024;
	u8 *buf = (u8*)memalign(32, buf_sz);
	if (!buf)
	{
		// Memory allocation failed.
		// TODO: Show an error.
		f_close(&in);
		free(md5_db);
		return;
	}

	// Start time.
	struct timeval tv;
	gettimeofday_rvlfix(&tv, NULL);
	const float time_start = tv.tv_sec + ((float)tv.tv_usec / 1000000.0f);

	u32 total_read = 0;
	u32 total_read_mb = 0;
	const u32 total_size = f_size(&in);
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
			free(buf);
			f_close(&in);
			return;
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
	const float time_end = tv.tv_sec + ((float)tv.tv_usec / 1000000.0f);
	const float time_diff = time_end - time_start;

	// Finished processing the MD5.
	// TODO: Verify against a list of MD5s.
	char md5_str[33];
	md5_to_str(md5_str, digest);
	ShowFinishScreen(time_diff, gi->Path, md5_str);
	ShowMD5DBStatus(md5_db_status);

	static const char press_a_button[] = "Press the A button to continue...";

	if (md5_db != NULL)
	{
		// Check the MD5 database.
		static const char checking_db[] = "Checking MD5 database...";
		PrintFormat(DEFAULT_SIZE, BLACK, STR_CONST_X(checking_db), 232+80, checking_db);
		// Render the text.
		GRRLIB_Render();
		ClearScreen();

		// TODO: Look up the MD5 in the database.
	}
	else
	{
		PrintFormat(DEFAULT_SIZE, BLACK, STR_CONST_X(press_a_button), 232+80, press_a_button);
		// Render the text.
		GRRLIB_Render();
		ClearScreen();
	}

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
