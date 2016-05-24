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


#define MAX_TITLES		740		// That should cover every GC game
#define LINE_LENGTH 	61
#define MAX_ELEMENTS(x) ((sizeof((x))) / (sizeof((x)[0])))

typedef struct {
	const char titleID[6];
	const char titleName[LINE_LENGTH - 4];
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
	

char __title_list[MAX_TITLES][LINE_LENGTH] = {{0}};
static u32 title_count = 0;
static bool loaded = false;

s32 LoadTitles(void) {
	int c = 0, line_char = 0;
	FIL titles_txt;
	char buffer[LINE_LENGTH] = {0};
	if(f_open_char(&titles_txt,"titles.txt",FA_READ|FA_OPEN_EXISTING) != FR_OK)
		return 0;
	loaded = true;
	UINT read;
	do {
		if(f_read(&titles_txt,(void*)c,1,&read) != FR_OK || read == 0)
			c = EOF; //hack I know...
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
	f_close(&titles_txt);
	return title_count;
}

inline bool SearchTitles(const char *titleID, char *titleName) {
	if (!loaded) return false;
	int i;
	for(i=0; i < MAX_ELEMENTS(TriforceTitles); i++) { // Check for Triforce arcade games first
		if (!strncmp(titleID, TriforceTitles[i].titleID, 6)) {
			strcpy(titleName, TriforceTitles[i].titleName);
			gprintf("Found special title %s, replacing name with %s\r\n", titleID, TriforceTitles[i].titleName);
			return true;
		}
	}
	for(i=0; i < title_count; i++) {
		if (!strncmp(titleID, __title_list[i], 3)) {
			strcpy(titleName, __title_list[i] + 4);
			return true;
		}
	}
	return false;
}
