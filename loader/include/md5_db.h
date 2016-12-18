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

#ifndef __MD5_DB_H__
#define __MD5_DB_H__

#include "global.h"

// MD5 database status.
typedef enum {
	MD5_DB_OK = 0,
	MD5_DB_NOT_FOUND,
	MD5_DB_TOO_BIG,
	MD5_DB_NO_MEM,
	MD5_DB_READ_ERROR,
} MD5_DB_Status;

// MD5 database.
typedef struct _MD5_DB_t {
	char *db;	// Allocated database.
	u32 size;	// Size of db.
} MD5_DB_t;

/**
 * Load the MD5 database.
 * @param pDB Pointer to MD5_DB_t to store the database in.
 * @return MD5_DB_Status
 */
MD5_DB_Status LoadMD5Database(MD5_DB_t *pDB);

/**
 * Free an MD5 database.
 * @param pDB Pointer to MD5_DB_t containing a loaded database.
 */
void FreeMD5Database(MD5_DB_t *pDB);

/**
 * Find an MD5 in the MD5 database.
 * @param pDB		[in] MD5 database in memory.
 * @param md5_str	[in] MD5 string in lowercase ASCII. (32 chars + NULL)
 * @return Copy of the MD5 line from the database, NULL-terminated; NULL if not found. (Must be freed after use!)
 */
char *FindMD5(const MD5_DB_t *pDB, const char *md5_str);

#endif /* __MD5_DB_H__ */
