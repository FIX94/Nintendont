
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
	FILE *f = fopen(filename, "rb");
	if(f == NULL)
	{
		snprintf(filename, sizeof(filename), "usb:/controllers/%04X_%04X.ini", DeviceVID, DevicePID);
		f = fopen(filename, "rb");
		if(f == NULL)
		{
			f = fopen("sd:/controller.ini", "rb");
			if(f == NULL)
			{
				fopen("sd:/controller.ini.ini", "rb");
				if(f == NULL)
				{
					f = fopen("usb:/controller.ini", "rb");
					if(f == NULL)
						f = fopen("usb:/controller.ini.ini", "rb");
				}
			}
		}
	}
	if(f != NULL)
	{
		fseek(f, 0, SEEK_END);
		size_t fsize = ftell(f);
		rewind(f);
		fread((void*)HID_CFG_FILE, 1, fsize, f);
		DCFlushRange((void*)HID_CFG_FILE, fsize);
		fclose(f);
		*(vu32*)HID_CFG_SIZE = fsize;
	}
	else
		*(vu32*)HID_CFG_SIZE = 0;
	*(vu32*)HID_CHANGE = 0;
}
