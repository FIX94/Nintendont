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
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <network.h>
#include <fat.h>
#include <dirent.h>

#include "exi.h"
#include "FPad.h"
#include "font.h"
#include "global.h"
#include "http.h"
#include "update.h"
#include "../../common/include/NintendontVersion.h"

extern char launch_dir[MAXPATHLEN];

// Please don't delete any commented code, it's for later

typedef struct {
	const char url[85];
	const char text[30];
	const char filename[30];
	const u32 max_size;
} downloads_t;

typedef enum {
	DOWNLOAD_NINTENDONT = 0,
	DOWNLOAD_TITLES,
	//DOWNLOAD_CONTROLLERS,
	DOWNLOAD_VERSION
} DOWNLOADS;

static const downloads_t Downloads[] = {
	{"http://nintendon-t.googlecode.com/svn/trunk/loader/loader.dol", "Updating Nintendont", "boot.dol", 1 << 21}, // 4MB
	{"http://nintendon-t.googlecode.com/svn/trunk/nintendont/titles.txt", "Updating titles.txt", "titles.txt", 1 << 19}, // 512KB
	//{"http://nintendon-t.googlecode.com/svn/trunk/controllerconfigs/controllers.zip", "Updating controllers.zip", "controllers.zip", 1 << 14}, // 16KB
	{"http://nintendon-t.googlecode.com/svn/trunk/common/include/NintendontVersion.h", "Checking Latest Version", "", 1 << 10} // 1KB
};

static inline bool LatestVersion(int *major, int *minor, int line) {
	u32 http_status = 0;
	u8* outbuf = NULL;
	u32 filesize;
	PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, Downloads[DOWNLOAD_VERSION].text);
	http_request(Downloads[DOWNLOAD_VERSION].url, Downloads[DOWNLOAD_VERSION].max_size);
	
	http_get_result(&http_status, &outbuf, &filesize);
	
	if (((int)*outbuf & 0xF0000000) == 0xF0000000) 
	{
		if (outbuf != NULL) free(outbuf);
		return false;
	}
	sscanf((char*)outbuf, " #ifndef %*s #define %*s #define NIN_MAJOR_VERSION %i #define NIN_MINOR_VERSION %i", major, minor);
	gprintf("major = %i, minor = %i\r\n", *major, *minor);
	if (outbuf != NULL) free(outbuf);
	return (*major + *minor) > 0;

}

static s32 Download(DOWNLOADS download_number)  {
	ClearScreen();

	int line = 1;
	int ret, major = 0, minor = 0;
	u32 http_status = 0;
	u8* outbuf = NULL;
	u32 filesize;
	char filepath[MAXPATHLEN];
	FILE *file;
	bool dir_argument_exists = strlen(launch_dir);
	snprintf(filepath, sizeof(filepath), "%s%s", dir_argument_exists ? launch_dir : "/apps/Nintendont/", Downloads[download_number].filename);
	PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, Downloads[download_number].text);
	line++;
	gprintf("Downloading %s to %s\r\n", Downloads[download_number].url, filepath);
	ret = net_init();
	if(ret < 0) {
		gprintf("Failed to init network\r\n");
		goto end;
	}
	gprintf("Network Initialized\r\n");
	PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, "Network Initialized");
	line++;
	if (download_number == DOWNLOAD_NINTENDONT) {
		ret = LatestVersion(&major, &minor, line);
		line++;
		if (ret && (major <= NIN_MAJOR_VERSION) && (minor <= NIN_MINOR_VERSION)) {
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, "You already have the latest version");
			if (outbuf != NULL) free(outbuf);
			net_deinit();
			sleep(4);
			return 0;
		} else {
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, "Downloading Nintendont v%i.%i", major, minor);
		}
		line++;
	}
	int i;
	for (i = 0; i < 10; i++) {
		ret = http_request(Downloads[download_number].url, Downloads[download_number].max_size);
		if (ret) break;
		if (i == 10) {
			gprintf("Error making http request\r\n");
			ret = -1;
			goto end;
		}
	}

	ret = http_get_result(&http_status, &outbuf, &filesize); 
	if(((int)*outbuf & 0xF0000000) == 0xF0000000) 
	{
		ret = -2;
		goto end;
	}
	PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, "Download Complete");
	line++;
	if (!dir_argument_exists) {
		gprintf("Creating new directory\r\n");
		mkdir("/apps/Nintendont", S_IWRITE|S_IREAD);
	}
	file = fopen(filepath, "wb");
	if(!file)
	{
		gprintf("File Error\r\n");
		ret = -3;
		goto end;
	} else {
		fwrite(outbuf, filesize, 1, file);
		fclose(file);
		PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, "Update Complete");
	}

end:
	if (ret != 1)
		PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, "Update Error: %i", ret);
	else
		PrintFormat(MENU_POS_X, MENU_POS_Y + 20*line, "Restart Nintendont to complete update");
	if (outbuf != NULL) free(outbuf);
	net_deinit();
	sleep(4);
	return ret;
}

void UpdateNintendont(void)  {
	ClearScreen();

	PrintFormat(MENU_POS_X, MENU_POS_Y + 20*1, "A: Download Nintendont");
	PrintFormat(MENU_POS_X, MENU_POS_Y + 20*2, "X: Download titles.txt");
	//PrintFormat(MENU_POS_X, MENU_POS_Y + 20*3, "Y: Download controllers.zip");
	PrintFormat(MENU_POS_X, MENU_POS_Y + 20*4, "B: Return to Settings");
	
	while(true) {
		FPAD_Update();
		
		if (FPAD_OK(0)) {
			Download(DOWNLOAD_NINTENDONT);
			break;
		}
		if (FPAD_X(0)) {
			Download(DOWNLOAD_TITLES);
			break;
		}
		//if (FPAD_Y(0)) {
		//	Download(DOWNLOAD_CONTROLLERS);
		//	break;
		//}
		if (FPAD_Cancel(0)) {
			break;
		}
	}
	ClearScreen();
	return;
}
