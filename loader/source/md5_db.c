/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U
MD5 database handler.

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

#include "md5_db.h"
#include "global.h"
#include "ff_utf8.h"
#include "md5.h"

#include <stdio.h>
#include <unistd.h>

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

/**
 * Load the MD5 database.
 * @param pDB Pointer to MD5_DB_t to store the database in.
 * @return MD5_DB_Status
 */
MD5_DB_Status LoadMD5Database(MD5_DB_t *pDB)
{
	// Determine the gcn_md5.txt path.
	// If loaded from network, launch_dir[] is empty,
	// so use /apps/Nintendont/ as a fallback.
	char filepath[MAXPATHLEN];
	snprintf(filepath, sizeof(filepath), "%sgcn_md5.txt",
		 launch_dir[0] != 0 ? launch_dir : "/apps/Nintendont/");

	FIL f_md5;
	pDB->db = NULL;
	pDB->size = 0;
	FRESULT res = f_open_char(&f_md5, filepath, FA_READ|FA_OPEN_EXISTING);
	if (res == FR_OK)
	{
		// Load the MD5 database. (Should be 1 MB or less.)
		static const u32 md5_db_size_max = 1024*1024;
		if (f_size(&f_md5) > md5_db_size_max)
		{
			// Database is too big.
			f_close(&f_md5);
			return MD5_DB_TOO_BIG;
		}
	
		pDB->size = (u32)f_size(&f_md5);
		pDB->db = (char*)memalign(32, pDB->size);
		if (!pDB->db)
		{
			// Memory allocation failed.
			f_close(&f_md5);
			return MD5_DB_NO_MEM;
		}

		UINT read;
		FRESULT res = f_read(&f_md5, pDB->db, pDB->size, &read);
		f_close(&f_md5);
		if (res != FR_OK || read != pDB->size)
		{
			// Read error.
			free(pDB->db);
			pDB->db = NULL;
			pDB->size = 0;
			return MD5_DB_READ_ERROR;
		}
	}
	else if (res == FR_NO_FILE || res == FR_NO_PATH)
	{
		// File not found.
		return MD5_DB_NOT_FOUND;
	}
	else
	{
		// Some other error...
		// TODO: Better error code?
		return MD5_DB_READ_ERROR;
	}

	// Database loaded.
	return MD5_DB_OK;
}

/**
 * Free an MD5 database.
 * @param pDB Pointer to MD5_DB_t containing a loaded database.
 */
void FreeMD5Database(MD5_DB_t *pDB)
{
	free(pDB->db);
	pDB->db = NULL;
}
