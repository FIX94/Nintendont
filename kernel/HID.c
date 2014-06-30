/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

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
#include "HID.h"
#include "ff.h"
#include "Config.h"
#include "hidmem.h"

#ifndef DEBUG_HID
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

static u8 ss_led_pattern[8] = {0x0, 0x02, 0x04, 0x08, 0x10, 0x12, 0x14, 0x18};

s32 HIDHandle = 0;
u32 PS3LedSet = 0;
u32 DeviceID  = 0;
u32 bEndpointAddress = 0;
u32 wMaxPacketSize = 0;
u32 MemPacketSize = 0;
u8 *Packet = (u8*)NULL;

unsigned char rawData[] =
{
    0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x27, 0x10, 0x00, 0x32, 
    0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 
} ;

req_args *req = (req_args *)NULL;
req_args *ps3req = (req_args *)NULL;
char *ps3buf = (char*)NULL;
s32 HIDInit( void )
{
	s32 ret;
	dbgprintf("HIDInit()\r\n");

	req = (req_args*)malloca( sizeof(req_args), 32 );
	memset32(req, 0, sizeof(req_args));

	ps3req = (req_args*)malloca( sizeof(req_args), 32 );
	memset32(req, 0, sizeof(req_args));

	ps3buf = (char*)malloca( 64, 32 );
	memset32(ps3buf, 0, 64);
	memcpy(ps3buf, rawData, sizeof(rawData));

	HIDHandle = IOS_Open("/dev/usb/hid", 0 );

	char *HIDHeap = (char*)malloca( 0x600, 32 );
	memset32( HIDHeap, 0, 0x600 );

	BootStatusError(8, 1);
	ret = IOS_Ioctl( HIDHandle, /*GetDeviceChange*/0, NULL, 0, HIDHeap, 0x600 );
	BootStatusError(8, 0);
	if( ret < 0 )
	{
		dbgprintf("HID:GetDeviceChange():%d\r\n", ret );
		free(HIDHeap);
		return -1;
	}

	DeviceID	= *(vu32*)(HIDHeap+4);
	dbgprintf("HID:DeviceID:%u\r\n", DeviceID );
	dbgprintf("HID:VID:%04X PID:%04X\r\n", *(vu16*)(HIDHeap+0x10), *(vu16*)(HIDHeap+0x12) );

	u32 Offset = 8;

	u32 DeviceDescLength    = *(vu8*)(HIDHeap+Offset);
	Offset += (DeviceDescLength+3)&(~3);

	u32 ConfigurationLength = *(vu8*)(HIDHeap+Offset);
	Offset += (ConfigurationLength+3)&(~3);

	u32 InterfaceDescLength = *(vu8*)(HIDHeap+Offset);
	Offset += (InterfaceDescLength+3)&(~3);

	u32 EndpointDescLengthO = *(vu8*)(HIDHeap+Offset);

	bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);

	if( (bEndpointAddress & 0xF0) != 0x80 )
		Offset += (EndpointDescLengthO+3)&(~3);

	bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);
	wMaxPacketSize   = *(vu16*)(HIDHeap+Offset+4);

	dbgprintf("HID:bEndpointAddress:%02X\r\n", bEndpointAddress );
	dbgprintf("HID:wMaxPacketSize  :%u\r\n", wMaxPacketSize );

	if( *(vu16*)(HIDHeap+0x10) == 0x054c && *(vu16*)(HIDHeap+0x12) == 0x0268 )
	{
		dbgprintf("HID:PS3 Dualshock Controller detected\r\n");
		MemPacketSize = SS_DATA_LEN;
		HIDPS3Init();
		HIDPS3SetRumble( 0, 0, 0, 0 );
	}

//Load controller config
	FIL f;
	u32 read;

	ret = f_open( &f, "/controller.ini", FA_OPEN_EXISTING|FA_READ);
	if( ret != FR_OK )
	{
		dbgprintf("HID:Failed to open config file:%u\r\n", ret );
		free(HIDHeap);
		return -2;
	}
	char *Data = (char*)malloc( f.fsize + 1 );
	f_read( &f, Data, f.fsize, &read );
	Data[f.fsize] = 0x00;	//null terminate the file
	f_close(&f);

	HID_CTRL->VID = ConfigGetValue( Data, "VID", 0 );
	HID_CTRL->PID = ConfigGetValue( Data, "PID", 0 );

	if( *(vu16*)(HIDHeap+0x10) != HID_CTRL->VID || *(vu16*)(HIDHeap+0x12) != HID_CTRL->PID )
	{
		dbgprintf("HID:Config does not match device VID/PID\r\n");
		dbgprintf("HID:Config VID:%04X PID:%04X\r\n", HID_CTRL->VID, HID_CTRL->PID );
		free(Data);
		free(HIDHeap);
		return -3;
	}

	HID_CTRL->DPAD		= ConfigGetValue( Data, "DPAD", 0 );
	HID_CTRL->DigitalLR	= ConfigGetValue( Data, "DigitalLR", 0 );
	HID_CTRL->Polltype	= ConfigGetValue( Data, "Polltype", 0 );
	HID_CTRL->MultiIn	= ConfigGetValue( Data, "MultiIn", 0 );

	if( HID_CTRL->MultiIn )
	{
		HID_CTRL->MultiInValue= ConfigGetValue( Data, "MultiInValue", 0 );

		dbgprintf("HID:MultIn:%u\r\n", HID_CTRL->MultiIn );
		dbgprintf("HID:MultiInValue:%u\r\n", HID_CTRL->MultiInValue );
	}

	if( HID_CTRL->Polltype == 0 )
		MemPacketSize = 128;
	else
		MemPacketSize = wMaxPacketSize;

	if( HID_CTRL->DPAD > 1 )
	{
		dbgprintf("HID: %u is an invalid DPAD value\r\n", HID_CTRL->DPAD );
		free(Data);
		free(HIDHeap);
		return -5;
	}

	HID_CTRL->Power.Offset	= ConfigGetValue( Data, "Power", 0 );
	HID_CTRL->Power.Mask	= ConfigGetValue( Data, "Power", 1 );

	HID_CTRL->A.Offset	= ConfigGetValue( Data, "A", 0 );
	HID_CTRL->A.Mask	= ConfigGetValue( Data, "A", 1 );

	HID_CTRL->B.Offset	= ConfigGetValue( Data, "B", 0 );
	HID_CTRL->B.Mask	= ConfigGetValue( Data, "B", 1 );

	HID_CTRL->X.Offset	= ConfigGetValue( Data, "X", 0 );
	HID_CTRL->X.Mask	= ConfigGetValue( Data, "X", 1 );

	HID_CTRL->Y.Offset	= ConfigGetValue( Data, "Y", 0 );
	HID_CTRL->Y.Mask	= ConfigGetValue( Data, "Y", 1 );

	HID_CTRL->Z.Offset	= ConfigGetValue( Data, "Z", 0 );
	HID_CTRL->Z.Mask	= ConfigGetValue( Data, "Z", 1 );

	HID_CTRL->L.Offset	= ConfigGetValue( Data, "L", 0 );
	HID_CTRL->L.Mask	= ConfigGetValue( Data, "L", 1 );

	HID_CTRL->R.Offset	= ConfigGetValue( Data, "R", 0 );
	HID_CTRL->R.Mask	= ConfigGetValue( Data, "R", 1 );

	HID_CTRL->S.Offset	= ConfigGetValue( Data, "S", 0 );
	HID_CTRL->S.Mask	= ConfigGetValue( Data, "S", 1 );

	HID_CTRL->Left.Offset	= ConfigGetValue( Data, "Left", 0 );
	HID_CTRL->Left.Mask		= ConfigGetValue( Data, "Left", 1 );

	HID_CTRL->Down.Offset	= ConfigGetValue( Data, "Down", 0 );
	HID_CTRL->Down.Mask		= ConfigGetValue( Data, "Down", 1 );

	HID_CTRL->Right.Offset	= ConfigGetValue( Data, "Right", 0 );
	HID_CTRL->Right.Mask	= ConfigGetValue( Data, "Right", 1 );

	HID_CTRL->Up.Offset		= ConfigGetValue( Data, "Up", 0 );
	HID_CTRL->Up.Mask		= ConfigGetValue( Data, "Up", 1 );

	if( HID_CTRL->DPAD )
	{
		HID_CTRL->RightUp.Offset	= ConfigGetValue( Data, "RightUp", 0 );
		HID_CTRL->RightUp.Mask		= ConfigGetValue( Data, "RightUp", 1 );

		HID_CTRL->DownRight.Offset	= ConfigGetValue( Data, "DownRight", 0 );
		HID_CTRL->DownRight.Mask	= ConfigGetValue( Data, "DownRight", 1 );

		HID_CTRL->DownLeft.Offset	= ConfigGetValue( Data, "DownLeft", 0 );
		HID_CTRL->DownLeft.Mask		= ConfigGetValue( Data, "DownLeft", 1 );

		HID_CTRL->UpLeft.Offset		= ConfigGetValue( Data, "UpLeft", 0 );
		HID_CTRL->UpLeft.Mask		= ConfigGetValue( Data, "UpLeft", 1 );
	}

	HID_CTRL->StickX.Offset		= ConfigGetValue( Data, "StickX", 0 );
	HID_CTRL->StickX.DeadZone	= ConfigGetValue( Data, "StickX", 1 );
	HID_CTRL->StickX.Radius		= ConfigGetDecValue( Data, "StickX", 2 );
	if (HID_CTRL->StickX.Radius == 0)
		HID_CTRL->StickX.Radius = 80;
	HID_CTRL->StickX.Radius = (u64)HID_CTRL->StickX.Radius * 1280 / (128 - HID_CTRL->StickX.DeadZone);	//adjust for DeadZone
//		dbgprintf("HID:StickX:  Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->StickX.Offset, HID_CTRL->StickX.DeadZone, HID_CTRL->StickX.Radius);

	HID_CTRL->StickY.Offset		= ConfigGetValue( Data, "StickY", 0 );
	HID_CTRL->StickY.DeadZone	= ConfigGetValue( Data, "StickY", 1 );
	HID_CTRL->StickY.Radius		= ConfigGetDecValue( Data, "StickY", 2 );
	if (HID_CTRL->StickY.Radius == 0)
		HID_CTRL->StickY.Radius = 80;
	HID_CTRL->StickY.Radius = (u64)HID_CTRL->StickY.Radius * 1280 / (128 - HID_CTRL->StickY.DeadZone);	//adjust for DeadZone
//		dbgprintf("HID:StickY:  Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->StickY.Offset, HID_CTRL->StickY.DeadZone, HID_CTRL->StickY.Radius);

	HID_CTRL->CStickX.Offset	= ConfigGetValue( Data, "CStickX", 0 );
	HID_CTRL->CStickX.DeadZone	= ConfigGetValue( Data, "CStickX", 1 );
	HID_CTRL->CStickX.Radius	= ConfigGetDecValue( Data, "CStickX", 2 );
	if (HID_CTRL->CStickX.Radius == 0)
		HID_CTRL->CStickX.Radius = 80;
	HID_CTRL->CStickX.Radius = (u64)HID_CTRL->CStickX.Radius * 1280 / (128 - HID_CTRL->CStickX.DeadZone);	//adjust for DeadZone
//		dbgprintf("HID:CStickX: Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->CStickX.Offset, HID_CTRL->CStickX.DeadZone, HID_CTRL->CStickX.Radius);

	HID_CTRL->CStickY.Offset	= ConfigGetValue( Data, "CStickY", 0 );
	HID_CTRL->CStickY.DeadZone	= ConfigGetValue( Data, "CStickY", 1 );
	HID_CTRL->CStickY.Radius	= ConfigGetDecValue( Data, "CStickY", 2 );
	if (HID_CTRL->CStickY.Radius == 0)
		HID_CTRL->CStickY.Radius = 80;
	HID_CTRL->CStickY.Radius = (u64)HID_CTRL->CStickY.Radius * 1280 / (128 - HID_CTRL->CStickY.DeadZone);	//adjust for DeadZone
//		dbgprintf("HID:CStickY: Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->CStickY.Offset, HID_CTRL->CStickY.DeadZone, HID_CTRL->CStickY.Radius);

	HID_CTRL->LAnalog	= ConfigGetValue( Data, "LAnalog", 0 );
	HID_CTRL->RAnalog	= ConfigGetValue( Data, "RAnalog", 0 );
	free(Data);

	dbgprintf("HID:Config file for VID:%04X PID:%04X loaded\r\n", HID_CTRL->VID, HID_CTRL->PID );

	free(HIDHeap);

	IOS_Close(HIDHandle);

	Packet = (u8*)malloca(MemPacketSize, 32);
	memset32(Packet, 0, MemPacketSize);
	sync_after_write(Packet, MemPacketSize);

	memset32(HID_Packet, 0, MemPacketSize);
	sync_after_write(HID_Packet, MemPacketSize);

	return 0;
}
void HIDPS3Init()
{
	memset32( req, 0, sizeof( req_args ) );

	char *buf = (char*)malloca( 0x20, 32 );
	memset32( buf, 0, 0x20 );

	req->device_no				= DeviceID;
	req->control.bmRequestType	= USB_REQTYPE_INTERFACE_GET;
	req->control.bmRequest		= USB_REQ_GETREPORT;
	req->control.wValue			= (USB_REPTYPE_FEATURE<<8) | 0xf2;
	req->control.wIndex			= 0;
	req->control.wLength		= 17;
	req->data					= buf;

	s32 ret = IOS_Ioctl( HIDHandle, /*ControlMessage*/2, req, 32, 0, 0 );
	if( ret < 0 )
	{
		dbgprintf("HID:HIDPS3Init:IOS_Ioctl( %u, %u, %p, %u, %u, %u):%d\r\n", HIDHandle, 2, req, 32, 0, 0, ret );
		BootStatusError(-8, -6);
		mdelay(2000);
		Shutdown();
	}
	free(buf);
}
void HIDPS3SetLED( u8 led )
{
	ps3buf[10] = ss_led_pattern[led];
	sync_after_write(ps3buf, 64);

	ps3req->device_no				= DeviceID;
	ps3req->interrupt.dLength		= sizeof(rawData);
	ps3req->interrupt.endpoint		= 0x02;
	ps3req->data					= ps3buf;

	s32 ret = IOS_Ioctl( HIDHandle, /*InterruptMessageIN*/4, ps3req, 32, 0, 0 );
	if( ret < 0 ) 
		dbgprintf("ES:IOS_Ioctl():%d\r\n", ret );
}
void HIDPS3SetRumble( u8 duration_right, u8 power_right, u8 duration_left, u8 power_left)
{
	ps3buf[3] = power_left;
	ps3buf[5] = power_right;
	sync_after_write(ps3buf, 64);

	ps3req->device_no				= DeviceID;
	ps3req->interrupt.dLength		= sizeof(rawData);
	ps3req->interrupt.endpoint		= 0x02;
	ps3req->data					= ps3buf;

	s32 ret = IOS_Ioctl( HIDHandle, /*InterruptMessageIN*/4, ps3req, 32, 0, 0 );
	if( ret < 0 )
		dbgprintf("ES:IOS_Ioctl():%d\r\n", ret );
}

u32 HIDRumbleCurrent = 0, HIDRumbleLast = 0;
vu32 *MotorCommand = (u32*)0x13003010;
vu32 *HIDChan = (u32*)0x13002700;
void HIDPS3Read()
{
	s32 ret;

	req->device_no				= DeviceID;
	req->control.bmRequestType	= USB_REQTYPE_INTERFACE_GET;
	req->control.bmRequest		= USB_REQ_GETREPORT;
	req->control.wValue			= (USB_REPTYPE_INPUT<<8) | 0x1;
	req->control.wIndex			= 0x0;
	req->control.wLength		= SS_DATA_LEN;
	req->data					= Packet;

	ret = IOS_Ioctl( HIDHandle, /*ControlMessage*/2, req, 32, 0, 0 );
	if( ret < 0 )
	{
		dbgprintf("HID:HIDPS3Read:IOS_Ioctl( %u, %u, %p, %u, %u, %u):%d\r\n", HIDHandle, 2, req, 32, 0, 0, ret );
		//Shutdown();
	}

	if( !PS3LedSet && Packet[4] )
	{
		HIDPS3SetLED(1);
		PS3LedSet = 1;
	}
	sync_before_read((void*)MotorCommand,0x20);
	sync_before_read((void*)HIDChan,0x20);
	HIDRumbleCurrent = MotorCommand[*HIDChan] & 0x3;
	if( HIDRumbleLast != HIDRumbleCurrent )
	{
		HIDRumble( HIDRumbleCurrent );
		HIDRumbleLast = HIDRumbleCurrent;
	}
	memcpy(HID_Packet, Packet, SS_DATA_LEN);
	sync_after_write(HID_Packet, SS_DATA_LEN);
	return;
}
void HIDIRQRead()
{
	s32 ret;

	req->device_no				= DeviceID;
	req->interrupt.dLength		= wMaxPacketSize;
	req->interrupt.endpoint		= bEndpointAddress;
	req->data					= Packet;

retry:
	ret = IOS_Ioctl( HIDHandle, /*InterruptMessageIN*/3, req, 32, 0, 0 );
	if( ret < 0 )
	{
		dbgprintf("ES:HIDIRQRead:IOS_Ioctl():%d\r\n", ret );
		Shutdown();
	}
	if(HID_CTRL->MultiIn && Packet[0] != HID_CTRL->MultiInValue)
	{
		//udelay(500);
		goto retry;
	}
	memcpy(HID_Packet, Packet, wMaxPacketSize);
	sync_after_write(HID_Packet, wMaxPacketSize);
	return;
}

void HIDRumble( u32 Enable )
{
	if( HID_CTRL->Polltype == 0 )
	{
		switch( Enable )
		{
			case 0:	// stop
			case 2:	// hard stop
				HIDPS3SetRumble( 0, 0, 0, 0 );
			break;
			case 1: // start
				HIDPS3SetRumble( 0, 0xFF, 0, 1 );
			break;
		}
	}
}

u32 ConfigGetValue( char *Data, const char *EntryName, u32 Entry )
{
	char entryname[128];
	_sprintf( entryname, "\n%s=", EntryName );

	char *str = strstr( Data, entryname );
	if( str == (char*)NULL )
	{
		dbgprintf("Entry:\"%s\" not found!\r\n", EntryName );
		return 0;
	}

	str += strlen(entryname); // Skip '='

	char *strEnd = strchr( str, 0x0A );

	if( Entry == 0 )
	{
		return atox(str);

	} else if ( Entry == 1 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			dbgprintf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip ,

		return atox(str);
	} else if ( Entry == 2 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			dbgprintf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the first ,

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			dbgprintf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the second ,

		return atox(str);
	}

	return 0;
}

int atoi(char *s)
{
	int i=0;
	int val = 0;

	while (s[i] >= '0' && s[i] <= '9')
	{
		val = val*10 + s[i] - '0';
		i++;
	}
	return val;
}
u32 ConfigGetDecValue( char *Data, const char *EntryName, u32 Entry )
{
	char entryname[128];
	_sprintf( entryname, "\n%s=", EntryName );

	char *str = strstr( Data, entryname );
	if( str == (char*)NULL )
	{
		dbgprintf("Entry:\"%s\" not found!\r\n", EntryName );
		return 0;
	}

	str += strlen(entryname); // Skip '='

	char *strEnd = strchr( str, 0x0A );

	if( Entry == 0 )
	{
		return atoi(str);

	} else if ( Entry == 1 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			dbgprintf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip ,

		return atoi(str);
	} else if ( Entry == 2 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL  || str > strEnd )
		{
			dbgprintf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the first ,

		str = strstr( str, "," );
		if( str == (char*)NULL  || str > strEnd )
		{
			dbgprintf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the second ,

		return atoi(str);
	}

	return 0;
}

u32 HID_Run(void *arg)
{
	HIDHandle = IOS_Open("/dev/usb/hid", 0 );
	bool Polltype = HID_CTRL->Polltype;

	dbgprintf("HID_Run, waiting for signal\r\n");
	while(read32(0x13003004) == 0)
	{
		sync_before_read((void*)0x13003004, 0x20);
		mdelay(500);
	}
	//dbgprintf("Starting HID Thread!\r\n");
	while(1)
	{
		if(Polltype)
			HIDIRQRead();
		else
			HIDPS3Read();
		mdelay(33);	// about 29 times a second, also for other threads
	}
	IOS_Close(HIDHandle);
	return 0;
}
