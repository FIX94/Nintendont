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

s32 UpdateNintendont(const char* directory)  {
	gprintf("Updating\r\n");
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*2, "Updating Nintendont");
	int ret;
	u32 http_status = 0;
	u8* outbuf;
	u32 filesize;
	FILE *file;
	gprintf("Update directory: %s", directory);
	ret = net_init();
	if(ret < 0) {
		gprintf("Failed to init network\r\n");
		goto end;
	}
	gprintf("Network Initialized\r\n");
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*3, "Downloading");
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
	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*4, "Download Complete");
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
		PrintFormat( MENU_POS_X, MENU_POS_Y + 20*5, "Update Complete");
	}

end:
	free(outbuf);
	if (ret != 1) PrintFormat( MENU_POS_X, MENU_POS_Y + 20*5, "Update Error: %i", ret);
	net_deinit();
	sleep(5);
	return ret;
}
