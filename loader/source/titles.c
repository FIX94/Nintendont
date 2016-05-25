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
#include <ogcsys.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>

#include "font.h"
#include "global.h"
#include "exi.h"


#define MAX_TITLES	740		// That should cover every GC game
#define LINE_LENGTH	64		// Max is actually 61, but this improves performance.
#define MAX_ELEMENTS(x) ((sizeof((x))) / (sizeof((x)[0])))

extern char launch_dir[MAXPATHLEN];

typedef struct {
	const char titleID[6];
	const char titleName[LINE_LENGTH];
} SpecialTitles_t;

static const SpecialTitles_t TriforceTitles[] = {
	{"GGPE01", "Mario Kart Arcade GP"},
	{"GGPJ01", "Mario Kart Arcade GP"},
	{"GGPP01", "Mario Kart Arcade GP"},
	{"GGPE02", "Mario Kart Arcade GP 2"},
	{"GGPJ02", "Mario Kart Arcade GP 2"},
	{"GGPP02", "Mario Kart Arcade GP 2"},
	{"GFZE8P", "F-Zero AX"},
	{"GFZP8P", "F-Zero AX"},
	{"GFZJ8P", "F-Zero AX"},
	{"GVSJ9P", "Virtua Striker 4 Ver.2006"},
	{"GVS46J", "Virtua Striker 4 Ver.2006"},
	{"GVS46E", "Virtua Striker 4 Ver.2006"}
};


static char __title_list[MAX_TITLES][LINE_LENGTH] = {{0}};
static u32 title_count = 0;
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

s32 LoadTitles(void)
{
	// Determine the titles.txt path.
	// If loaded from network, launch_dir[] is empty,
	// so use /apps/Nintendont/ as a fallback.
	char filepath[MAXPATHLEN];
	snprintf(filepath, sizeof(filepath), "%stitles.txt",
		 launch_dir[0] != 0 ? launch_dir : "/apps/Nintendont/");

	FILE *titles_txt = fopen(filepath, "rb");
	if (!titles_txt)
		return 0;

	char *cur_title = &__title_list[0][0];
	title_count = 0;
	loaded = true;
	do {
		if (!fgets(cur_title, LINE_LENGTH, titles_txt))
			break;

		// Trim newlines and/or carriage returns.
		int len = (int)strlen(cur_title)-1;
		for (; len > 5; len--) {
			if (cur_title[len] == '\n' || cur_title[len] == '\r') {
				cur_title[len] = 0;
			} else {
				break;
			}
		}

		if (len > 5) {
			// Valid title.
			title_count++;
			cur_title = &__title_list[title_count][0];
		}
	} while (!feof(titles_txt) && title_count < MAX_TITLES);

	// Sort the titles so we can do a binary search later.
	// __title_list[] format: "ID3-Title"
	qsort(__title_list, title_count, LINE_LENGTH, compare_title);

	fclose(titles_txt);
	return title_count;
}

/**
 * Find a title in the titles database.
 * Loaded from titles.txt, plus special exceptions for Triforce.
 * @param titleID Title ID. (ID6)
 * @return Title, or null pointer if not found.
 * WARNING: DO NOT FREE the returned title!
 */
const char *SearchTitles(const char *titleID) {
	if (!loaded) {
		// Titles haven't been loaded.
		return NULL;
	}

	// Check for Triforce arcade games first.
	int i;
	for (i = 0; i < MAX_ELEMENTS(TriforceTitles); i++) {
		if (!strncmp(titleID, TriforceTitles[i].titleID, 6)) {
			gprintf("Found special title %.6s, replacing name with %s\r\n", titleID, TriforceTitles[i].titleName);
			return TriforceTitles[i].titleName;
		}
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
