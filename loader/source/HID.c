
#include <gccore.h>
#include "exi.h"
#include "HID.h"

#define HID_STATUS 0xD3003440
#define HID_CHANGE (HID_STATUS+4)
#define HID_CFG_SIZE (HID_STATUS+8)

#define HID_CFG_FILE 0x93003460

void HIDUpdateRegisters()
{
	if(*(vu32*)HID_CHANGE == 0)
		return;
	u32 DeviceVID = *(vu32*)HID_CHANGE;
	u32 DevicePID = *(vu32*)HID_CFG_SIZE;

	/* I hope this covers all possible ini files */
	gprintf("Trying to get VID%04x PID%04x\n", DeviceVID, DevicePID);
	char filename[50];
	snprintf(filename, sizeof(filename), "sd:/controllers/%04X_%04X.ini", DeviceVID, DevicePID);
	FIL f;
	FRESULT res = FR_OK;
	if(f_open_char(&f,filename,FA_READ|FA_OPEN_EXISTING) != FR_OK)
	{
		snprintf(filename, sizeof(filename), "usb:/controllers/%04X_%04X.ini", DeviceVID, DevicePID);
		if(f_open_char(&f,filename,FA_READ|FA_OPEN_EXISTING) != FR_OK)
		{
			if(f_open_char(&f,"sd:/controller.ini",FA_READ|FA_OPEN_EXISTING) != FR_OK)
			{
				if(f_open_char(&f,"sd:/controller.ini.ini",FA_READ|FA_OPEN_EXISTING) != FR_OK)
				{
					if(f_open_char(&f,"usb:/controller.ini",FA_READ|FA_OPEN_EXISTING) != FR_OK)
						res = f_open_char(&f,"usb:/controller.ini.ini",FA_READ|FA_OPEN_EXISTING);
				}
			}
		}
	}
	if(res == FR_OK)
	{
		size_t fsize = f.obj.objsize;
		UINT read;
		f_read(&f, (void*)HID_CFG_FILE, fsize, &read);
		DCFlushRange((void*)HID_CFG_FILE, fsize);
		f_close(&f);
		*(vu32*)HID_CFG_SIZE = fsize;
	}
	else
		*(vu32*)HID_CFG_SIZE = 0;
	*(vu32*)HID_CHANGE = 0;
}
