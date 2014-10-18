#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <network.h>
#include <fat.h>

#include "exi.h"
#include "font.h"
#include "global.h"
#include "http.h"
#include "update.h"
#include "../../common/include/NintendontVersion.h"


static inline bool LatestVersion(int *major, int *minor) {
	u32 http_status = 0;
	u8* outbuf = NULL;
	u32 filesize;
	
	http_request("http://nintendon-t.googlecode.com/svn/trunk/common/include/NintendontVersion.h", 1 << 10);
	
	http_get_result(&http_status, &outbuf, &filesize); 
	
	if (((int)*outbuf & 0xF0000000) == 0xF0000000) 
	{
		if (outbuf != NULL) free(outbuf);
		return false;
	}
	sscanf((char*)outbuf, " #ifndef %*s #define %*s #define NIN_MAJOR_VERSION %i #define NIN_MINOR_VERSION %i", major, minor);

	if (outbuf != NULL) free(outbuf);
	return (*major + *minor) > 0;

}

s32 UpdateNintendont(const char* directory)  {
	gprintf("Updating\r\n");
	int line = 1;
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*line, "Updating Nintendont");
	line++;
	int ret, major = 0, minor = 0;
	u32 http_status = 0;
	u8* outbuf = NULL;
	u32 filesize;
	FILE *file;
	gprintf("Update directory: %s", directory);
	ret = net_init();
	if(ret < 0) {
		gprintf("Failed to init network\r\n");
		goto end;
	}
	gprintf("Network Initialized\r\n");
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*line, "Network Initialized");
	line++;
	ret = LatestVersion(&major, &minor);
	if (ret && (major <= NIN_MAJOR_VERSION) && (minor <= NIN_MINOR_VERSION)) {
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*line, "You already have the lastest version");
		line++;
		goto end;
	}
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*line, "Downloading v%i.%i", major, minor);
	line++;
	int i;
	for (i = 0; i < 10; i++) {
		ret = http_request("http://nintendon-t.googlecode.com/svn/trunk/loader/loader.dol", 1 << 30); // I doubt it will ever go over 1MB
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
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*line, "Download Complete");
	line++;
	if(strlen(directory) > 0) {
		file = fopen(directory, "wb");
	} else {
		file = fopen(DEFAULT_DIR, "wb");
	}
	if(!file)
	{
		gprintf("File Error\r\n");
		ret = -3;
		goto end;
	} else {
		fwrite(outbuf, filesize, 1, file);
		fclose(file);
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*line, "Update Complete");
	}

end:
	if (outbuf != NULL) free(outbuf);
	if (ret != 1) PrintFormat( MENU_POS_X, MENU_POS_Y + 20*line, "Update Error: %i", ret);
	net_deinit();
	sleep(5);
	return ret;
}
