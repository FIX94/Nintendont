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
#include <zlib.h>
#include <ogc/lwp_watchdog.h>

#include "exi.h"
#include "FPad.h"
#include "font.h"
#include "global.h"
#include "http.h"
#include "ssl.h"
#include "menu.h"
#include "update.h"
#include "unzip/miniunz.h"
#include "../../common/include/NintendontVersion.h"

extern char launch_dir[MAXPATHLEN];

typedef struct {
	const char url[96];
	const char text[30];
	const char filename[30];
	const u32 max_size;
} downloads_t;

typedef enum {
	DOWNLOAD_NINTENDONT = 0,
	DOWNLOAD_TITLES,
	DOWNLOAD_CONTROLLERS,
	DOWNLOAD_VERSION
} DOWNLOADS;

static const downloads_t Downloads[] = {
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/loader/loader.dol", "Updating Nintendont", "boot.dol", 0x400000}, // 4MB
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/nintendont/titles.txt", "Updating titles.txt", "titles.txt", 0x80000}, // 512KB
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/controllerconfigs/controllers.zip", "Updating controllers.zip", "controllers.zip", 0x8000}, // 32KB
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/common/include/NintendontVersion.h", "Checking Latest Version", "", 0x400} // 1KB
};

static int UnzipControllers(const char* filepath) {
	char unzip_directory[20];
	unzFile uf = unzOpen(filepath);
	if (uf==NULL)
	{
		gprintf("Cannot open %s, aborting\r\n", Downloads[DOWNLOAD_CONTROLLERS].filename);
		return -1;
	}
	gprintf("%s opened\n", Downloads[DOWNLOAD_CONTROLLERS].filename);
	
	sprintf(unzip_directory, "%s:/controllers", UseSD ? "sd" : "usb");
	mkdir(unzip_directory,S_IWRITE|S_IREAD); // attempt to make dir
	if(chdir(unzip_directory)) {
		gprintf("Error changing into %s, aborting\r\n", unzip_directory);
		return -2;
	}

	if (extractZip(uf,0,1,0)) {
		gprintf("Failed to extract %s\r\n", filepath);
		return -3;
	}

	unzCloseCurrentFile(uf);
	remove(Downloads[DOWNLOAD_CONTROLLERS].filename);
	return 1;
}

static inline bool LatestVersion(int *major, int *minor, int *current_line) {
	u32 http_status = 0;
	u8* outbuf = NULL;
	u32 filesize;
	int line = *current_line;

	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, Downloads[DOWNLOAD_VERSION].text);
	UpdateScreen();
	line++;
	if(!http_request(Downloads[DOWNLOAD_VERSION].url, Downloads[DOWNLOAD_VERSION].max_size)) {
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Failed to retrieve version");
		UpdateScreen();
		*current_line = line;
		return false;
	}
	

	http_get_result(&http_status, &outbuf, &filesize);
	
	if (((int)*outbuf & 0xF0000000) == 0xF0000000) 
	{
		if (outbuf != NULL) free(outbuf);
		*current_line = line;
		return false;
	}
	sscanf((char*)outbuf, " #ifndef %*s #define %*s #define NIN_MAJOR_VERSION %i #define NIN_MINOR_VERSION %i", major, minor);
	gprintf("major = %i, minor = %i\r\n", *major, *minor);
	if (outbuf != NULL) free(outbuf);
	if ((*major <= NIN_MAJOR_VERSION) && (*minor <= NIN_MINOR_VERSION)) {
		bool still_download = true;
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "You already have the latest version");
		line++;
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Download anyway? (A: Yes, B: No)");
		line++;
		UpdateScreen();
		while(true) {
			DrawBuffer();
			FPAD_Update();
			if (FPAD_Cancel(0)) {
				gprintf("Cancelling download\n");
				still_download = false;
				break;
			}
			if (FPAD_OK(0)) {
				gprintf("okay\n");
				break;
			}
			GRRLIB_Render();
		}
		if(!still_download) {
			*current_line = line;
			return false;
		}
	}
	*current_line = line;
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
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, Downloads[download_number].text);
	UpdateScreen();
	
	line++;
	gprintf("Downloading %s to %s\r\n", Downloads[download_number].url, filepath);
	ret = net_init();
	if(ret < 0) {
		gprintf("Failed to init network\r\n");
		goto end;
	}
	gprintf("Network Initialized\r\n");
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Network Initialized");
	UpdateScreen();
	ssl_init(); //only once needed
	line++;
	if (download_number == DOWNLOAD_NINTENDONT) {
		ret = LatestVersion(&major, &minor, &line);
		if (!ret) {
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Download Cancelled");
			UpdateScreen();
			if (outbuf != NULL) free(outbuf);
			net_deinit();
			sleep(4);
			return 0;
		}

		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Downloading Nintendont v%i.%i", major, minor);
		UpdateScreen();
		line++;
	}
	int i;
	for (i = 0; i <= 10; i++) {
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
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Download Complete");
	UpdateScreen();
	line++;
	if (!dir_argument_exists) {
		gprintf("Creating new directory\r\n");
		mkdir("/apps", S_IWRITE|S_IREAD);
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
		if (download_number == DOWNLOAD_CONTROLLERS) ret = UnzipControllers(filepath);
		if (ret == 1) {
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Update Complete");
			UpdateScreen();
			line++;
		}
	}

end:
	if (ret != 1)
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*line, "Update Error: %i", ret);
	else
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Restart Nintendont to complete update");
	UpdateScreen();
	if (outbuf != NULL) free(outbuf);
	net_deinit();
	sleep(4);
	return ret;
}

void UpdateNintendont(void) {
	int selected = 0;
	u64 delay = ticks_to_millisecs(gettime()) + 500;
	while(true) {
		ClearScreen();
		PrintInfo();
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 50, MENU_POS_Y + 20*5, "Download Nintendont");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 50, MENU_POS_Y + 20*6, "Download titles.txt");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 50, MENU_POS_Y + 20*7, "Download controllers.zip");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 50, MENU_POS_Y + 20*8, "Return to Settings");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 35, MENU_POS_Y + 20*(5+selected), ARROW_RIGHT);
		GRRLIB_Render();
		FPAD_Update();
		if (delay > ticks_to_millisecs(gettime())) continue;
		if (FPAD_Start(1)) {
			ShowMessageScreenAndExit("Returning to loader...", 0);
		}
		if (FPAD_OK(1)) {
			if (selected <= DOWNLOAD_CONTROLLERS)
				Download(selected);
			else
				break;
		}
		if (FPAD_Down(1)) {
			delay = ticks_to_millisecs(gettime()) + 150;
			selected++;
			if (selected > 3) selected = 0;
		}
		if (FPAD_Up(1)) {
			delay = ticks_to_millisecs(gettime()) + 150;
			selected--;
			if (selected < 0) selected = 3;
		}
		if (FPAD_Cancel(1)) {
			break;
		}
	}
	ClearScreen();
	return;
}
