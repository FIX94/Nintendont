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

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>

#include "font.h"
#include "global.h"
#include "exi.h"

#include "ff_utf8.h"


#define MAX_TITLES	740		// That should cover every GC game
#define LINE_LENGTH	64		// Max is actually 61, but this improves performance.
#define MAX_ELEMENTS(x) ((sizeof((x))) / (sizeof((x)[0])))

extern char launch_dir[MAXPATHLEN];

typedef struct {
	const char titleID[8];
	const char *titleName;
} SpecialTitles_t;

static const SpecialTitles_t TriforceTitles[] = {
	{"GFZE8P", "F-Zero AX"},
	{"GFZP8P", "F-Zero AX"},
	{"GFZJ8P", "F-Zero AX"},
	{"GGPE01", "Mario Kart Arcade GP"},
	{"GGPJ01", "Mario Kart Arcade GP"},
	{"GGPP01", "Mario Kart Arcade GP"},
	{"GGPE02", "Mario Kart Arcade GP 2"},
	{"GGPJ02", "Mario Kart Arcade GP 2"},
	{"GGPP02", "Mario Kart Arcade GP 2"},
	{"GPBJ8P", "Gekitou Pro Yakyuu"},
	{"GVS32J", "Virtua Striker 3 Ver. 2002"},
	{"GVS32E", "Virtua Striker 3 Ver. 2002"},
	{"GVS45J", "Virtua Striker 4"},
	{"GVS45E", "Virtua Striker 4"},
	{"GVSJ9P", "Virtua Striker 4 Ver. 2006"},
	{"GVS46J", "Virtua Striker 4 Ver. 2006"},
	{"GVS46E", "Virtua Striker 4 Ver. 2006"}
};


static char __title_list[MAX_TITLES][LINE_LENGTH] = {{0}};
static int title_count = 0;
static bool loaded = false;

/**
 * qsort() comparison function for titles.
 * Checks the ID3.
 * @param p1
 * @param p2
 */
static int compare_title(const void *p1, const void *p2)
{
       return strncmp((const char*)p1, (const char*)p2, 3);
}

/**
 * Load game titles from titles.txt.
 * @return Number of titles loaded.
 */
int LoadTitles(void)
{
	// Determine the titles.txt path.
	// If loaded from network, launch_dir[] is empty,
	// so use /apps/Nintendont/ as a fallback.
	char filepath[MAXPATHLEN];
	snprintf(filepath, sizeof(filepath), "%stitles.txt",
		 launch_dir[0] != 0 ? launch_dir : "/apps/Nintendont/");

	FIL titles_txt;
	if (f_open_char(&titles_txt, filepath, FA_READ|FA_OPEN_EXISTING) != FR_OK)
		return 0;

	char *cur_title = &__title_list[0][0];
	title_count = 0;
	loaded = true;

	// FIXME: Optimize title loading by reading chunks at a time
	// instead of single bytes.
	char c;
	int pos = 0;
	UINT read;
	do {
		if (f_read(&titles_txt, &c, 1, &read) != FR_OK || read == 0)
			c = 0;
		if (c == '\r') {
			continue;
		}

		if (c == '\n' || c == 0 || pos == (LINE_LENGTH - 1))
		{
			// End of the line.
			if (pos > 5) {
				// Valid title.
				*cur_title = 0;
				title_count++;
				cur_title = &__title_list[title_count][0];
				pos = 0;
			}
		}
		else
		{
			// Append the character.
			*cur_title++ = c;
			pos++;
		}
	} while (!f_eof(&titles_txt) && c != 0);
	f_close(&titles_txt);

	// Sort the titles so we can do a binary search later.
	// __title_list[] format: "ID3-Title"
	qsort(__title_list, title_count, LINE_LENGTH, compare_title);
	return title_count;
}

/**
 * Find a title in the titles database.
 * Loaded from titles.txt, plus special exceptions for Triforce.
 * @param titleID	[in] Title ID. (ID6)
 * @param pIsTriforce	[out,opt] Set to true if this is a Triforce title.
 * @return Title, or NULL if not found.
 * WARNING: DO NOT FREE the returned title!
 */
const char *SearchTitles(const char *titleID, bool *pIsTriforce)
{
	if (!loaded) {
		// Titles haven't been loaded.
		return NULL;
	}

	// Check for Triforce arcade games first.
	int i;
	for (i = 0; i < MAX_ELEMENTS(TriforceTitles); i++) {
		if (!strncmp(titleID, TriforceTitles[i].titleID, 6)) {
			gprintf("Found special title %.6s, replacing name with %s\r\n", titleID, TriforceTitles[i].titleName);
			if (pIsTriforce)
			{
				*pIsTriforce = true;
			}
			return TriforceTitles[i].titleName;
		}
	}

	// Not a Triforce title.
	if (pIsTriforce)
	{
		*pIsTriforce = false;
	}

	// Do a binary search for the ID3.
	// __title_list[] format: "ID3-Title"
	// Reference: http://www.programmingsimplified.com/c/source-code/c-program-binary-search
	// TODO: Do timing tests.
	int first = 0;
	int last = title_count - 1;
	int middle = (first + last) / 2;
	while (first <= last) {
		int res = strncmp(__title_list[middle], titleID, 3);
		if (res < 0) {
			first = middle + 1;
		} else if (res > 0) {
			last = middle - 1;
		} else {
			// Found a match.
			return __title_list[middle] + 4;
		}
		middle = (first + last) / 2;
	}

	// Title not found.
	return NULL;
}
