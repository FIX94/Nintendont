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
#include "../../common/include/NintendontVersion.h"

#include "ff_utf8.h"
#include "unzip/miniunz.h"
#include "unzip/ioapi.h"

extern char launch_dir[MAXPATHLEN];

typedef struct {
	const char *url;
	const char *text;
	const char *filename;
	const u32 max_size;
} downloads_t;

typedef enum {
	DOWNLOAD_NINTENDONT = 0,
	DOWNLOAD_TITLES,
	DOWNLOAD_CONTROLLERS,
	DOWNLOAD_GAMECUBE_MD5,
	DOWNLOAD_VERSION
} DOWNLOADS;

static const downloads_t Downloads[] = {
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/loader/loader.dol", "Updating Nintendont", "boot.dol", 0x400000}, // 4MB
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/nintendont/titles.txt", "Updating titles.txt", "titles.txt", 0x80000}, // 512KB
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/controllerconfigs/controllers.zip", "Updating controllers.zip", "controllers.zip", 0x8000}, // 32KB
	{"https://raw.githubusercontent.com/GerbilSoft/Nintendont/widescreen-hax.r422/nintendont/gcn_md5.zip", "Updating gcn_md5.txt", "gcn_md5.zip", 0x20000}, // 128 KB
	{"https://raw.githubusercontent.com/FIX94/Nintendont/master/common/include/NintendontVersion.h", "Checking Latest Version", "", 0x400} // 1KB
};

extern void changeToDefaultDrive();
static int UnzipFile(const char *dir, bool useDefaultDrive, DOWNLOADS download_number, const void *buf, size_t fSize) {
	char filepath[20];
	snprintf(filepath,20,"%x+%x",(u32)buf,(u32)fSize);
	unzFile uf = unzOpen(filepath);
	if (!uf) {
		gprintf("Cannot open %s, aborting\r\n", Downloads[download_number].filename);
		return -1;
	}
	gprintf("%s opened\n", Downloads[download_number].filename);

	// Use the default drive or the active drive?
	if (useDefaultDrive) {
		changeToDefaultDrive();
	} else {
		// Use the active drive.
		f_chdrive_char(UseSD ? "sd:" : "usb:");
	}

	f_mkdir_char(dir); // attempt to make dir
	if (f_chdir_char(dir) != FR_OK) {
		gprintf("Error changing into %s, aborting\r\n", dir);
		return -2;
	}

	if (extractZip(uf, 0, 1, 0)) {
		gprintf("Failed to extract %s\r\n", filepath);
		return -3;
	}

	unzCloseCurrentFile(uf);
	unzClose(uf);
	remove(Downloads[download_number].filename);
	changeToDefaultDrive();
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

	bool dir_argument_exists = (launch_dir[0] != 0);
	const char *dir = (dir_argument_exists ? launch_dir : "/apps/Nintendont/");

	snprintf(filepath, sizeof(filepath), "%s%s", dir, Downloads[download_number].filename);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, Downloads[download_number].text);
	UpdateScreen();

	// TODO: Use ioapi mem and skip writing the ZIP file to disk.
	line++;
	gprintf("Downloading %s to %s\r\n", Downloads[download_number].url, filepath);
	ret = net_init();
	if (ret < 0) {
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
	if (((int)*outbuf & 0xF0000000) == 0xF0000000) {
		ret = -2;
		goto end;
	}

	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Download Complete");
	UpdateScreen();
	line++;
	if (!dir_argument_exists) {
		gprintf("Creating new directory\r\n");
		f_mkdir_char("/apps");
		f_mkdir_char("/apps/Nintendont");
	}

	// Write the file to disk.
	if (download_number == DOWNLOAD_CONTROLLERS) {
		// controllers.zip needs to be decompressed to the
		// active drive, since the kernel uses it.
		ret = UnzipFile("/controllers", false, DOWNLOAD_CONTROLLERS, outbuf, filesize);
	}
	else if (download_number == DOWNLOAD_GAMECUBE_MD5) {
		// gcn_md5.zip needs to be decompressed to the
		// loader's drive, since it's used by the verifier.

		// Need a directory without a trailing '/'.
		char *dirNoSlash = strdup(dir);
		size_t len = strlen(dirNoSlash);
		if (len > 1 && dirNoSlash[len-1] == '/')
			dirNoSlash[len-1] = 0;

		ret = UnzipFile(dirNoSlash, true, DOWNLOAD_GAMECUBE_MD5, outbuf, filesize);
		free(dirNoSlash);
	}
	else
	{
		FIL file;
		if (f_open_char(&file, filepath, FA_WRITE|FA_CREATE_ALWAYS) != FR_OK) {
			gprintf("File Error\r\n");
			ret = -3;
			goto end;
		} else {
			// Reserve space in the file.
			f_expand(&file, filesize, 1);

			// Write the file.
			UINT wrote;
			f_write(&file, outbuf, filesize, &wrote);
			f_close(&file);
			FlushDevices();
			ret = 1;
		}
	}
	if (ret == 1) {
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Update Complete");
		UpdateScreen();
		line++;
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
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 50, MENU_POS_Y + 20*8, "Download gcn_md5.txt");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 50, MENU_POS_Y + 20*9, "Return to Settings");
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 35, MENU_POS_Y + 20*(5+selected), ARROW_RIGHT);
		GRRLIB_Render();
		FPAD_Update();
		if (delay > ticks_to_millisecs(gettime())) continue;
		if (FPAD_Start(1)) {
			ShowMessageScreenAndExit("Returning to loader...", 0);
		}
		if (FPAD_OK(1)) {
			if (selected <= DOWNLOAD_GAMECUBE_MD5)
				Download(selected);
			else
				break;
		}
		if (FPAD_Down(1)) {
			delay = ticks_to_millisecs(gettime()) + 150;
			selected++;
			if (selected > 4) selected = 0;
		}
		if (FPAD_Up(1)) {
			delay = ticks_to_millisecs(gettime()) + 150;
			selected--;
			if (selected < 0) selected = 4;
		}
		if (FPAD_Cancel(1)) {
			break;
		}
	}
	ClearScreen();
	return;
}
