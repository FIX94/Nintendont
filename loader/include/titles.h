/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2014  JoostinOnline

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

#ifndef __TITLES_H__
#define __TITLES_H__

/**
 * Load game titles from titles.txt.
 * @return Number of titles loaded.
 */
int LoadTitles(void);

/**
 * Find a title in the titles database.
 * Loaded from titles.txt, plus special exceptions for Triforce.
 * @param titleID	[in] Title ID. (ID6)
 * @param pIsTriforce	[out,opt] Set to true if this is a Triforce title.
 * @return Title, or NULL if not found.
 * WARNING: DO NOT FREE the returned title!
 */
const char *SearchTitles(const char *titleID, bool *pIsTriforce);

#endif
