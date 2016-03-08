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

#include "font.h"
#include "global.h"
#include "exi.h"


#define MAX_TITLES	740		// That should cover every GC game
#define LINE_LENGTH	64		// Max is actually 61, but this improves performance.
#define MAX_ELEMENTS(x) ((sizeof((x))) / (sizeof((x)[0])))

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

s32 LoadTitles(void) {
	int c = 0, line_char = 0;
	FILE *titles_txt = NULL;
	char buffer[LINE_LENGTH+4] = {0};
	titles_txt = fopen("titles.txt", "rb");
	if (titles_txt == NULL) return 0;
	loaded = true;
	do {
		c = fgetc(titles_txt);
		if (c == '\r') continue;
		buffer[line_char] = c;

		if ((c == '\n') || (line_char == LINE_LENGTH - 1)) {
			buffer[line_char] = 0;
			if (line_char > 5) {
				snprintf(__title_list[title_count], LINE_LENGTH, buffer);
				title_count++;
			}
			line_char = 0;
		} else line_char++;
	} while (c != EOF);

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
	// FIXME: This is a linear search, which can be slow...
	if (!loaded) return NULL;
	int i;
	for(i=0; i < MAX_ELEMENTS(TriforceTitles); i++) { // Check for Triforce arcade games first
		if (!strncmp(titleID, TriforceTitles[i].titleID, 6)) {
			gprintf("Found special title %.6s, replacing name with %s\r\n", titleID, TriforceTitles[i].titleName);
			return TriforceTitles[i].titleName;
		}
	}
	for(i=0; i < title_count; i++) {
		// __title_list[] format: ID3-Title
		if (!strncmp(titleID, __title_list[i], 3)) {
			return __title_list[i] + 4;
		}
	}

	// Title not found.
	return NULL;
}
