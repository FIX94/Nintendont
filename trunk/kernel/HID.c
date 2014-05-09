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
#include "config.h"
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
u32 wMaxPacketSize	 = 0;
u8 *Packet = (u8*)NULL;

req_args *req = (req_args *)NULL;

s32 HIDInit( void )
{
	s32 ret;
	dbgprintf("HIDInit()\r\n");
	HIDHandle = IOS_Open("/dev/usb/hid", 0 );

	char *HIDHeap = (char*)malloca( 0x600, 32 );
	memset32( HIDHeap, 0, 0x600 );

	req	= (req_args*)malloca( sizeof(req_args), 32 );
	BootStatusError(8, 1);
	ret = IOS_Ioctl( HIDHandle, /*GetDeviceChange*/0, NULL, 0, HIDHeap, 0x600 );
	BootStatusError(8, 0);
	if( ret < 0 )
	{
		dbgprintf("HID:GetDeviceChange():%d\r\n", ret );
		return -1;
	}

	DeviceID	= *(vu32*)(HIDHeap+4);
	HIDHandle	= HIDHandle;

	dbgprintf("HID:DeviceID:%u\r\n", DeviceID );
	dbgprintf("HID:VID:%04X PID:%04X\r\n", *(vu16*)(HIDHeap+0x10), *(vu16*)(HIDHeap+0x12) );
			
	u32 Offset = 8;
		
	u32 DeviceDescLength	= *(vu8*)(HIDHeap+Offset);
	Offset += (DeviceDescLength+3)&(~3);

	u32 ConfigurationLength = *(vu8*)(HIDHeap+Offset);
	Offset += (ConfigurationLength+3)&(~3);
		
	u32 InterfaceDescLength	= *(vu8*)(HIDHeap+Offset);
	Offset += (InterfaceDescLength+3)&(~3);
		
	u32 EndpointDescLengthO	= *(vu8*)(HIDHeap+Offset);
		
	bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);

	if( (bEndpointAddress & 0xF0) != 0x80 )
		Offset += (EndpointDescLengthO+3)&(~3);

	bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);
	wMaxPacketSize	 = *(vu16*)(HIDHeap+Offset+4);

	dbgprintf("HID:bEndpointAddress:%02X\r\n", bEndpointAddress );
	dbgprintf("HID:wMaxPacketSize  :%u\r\n", wMaxPacketSize );

	if( *(vu16*)(HIDHeap+0x10) == 0x054c && *(vu16*)(HIDHeap+0x12) == 0x0268 )
	{
		dbgprintf("HID:PS3 Dualshock Controller detected\r\n");

		HIDPS3Init();

		HIDPS3SetRumble( 0, 0, 0, 0 );

		Packet = (u8*)malloca(SS_DATA_LEN, 32);
	}

//Load controller config
	FIL f;
	u32 read;

	ret = f_open( &f, "/controller.ini", FA_OPEN_EXISTING|FA_READ);
	if( ret == FR_OK )
	{
		char *Data = (char*)malloc( f.fsize );
		f_read( &f, Data, f.fsize, &read );
		f_close(&f);

		HID_CTRL->VID = ConfigGetValue( Data, "VID", 0 );
		HID_CTRL->PID = ConfigGetValue( Data, "PID", 0 );

		if( *(vu16*)(HIDHeap+0x10) != HID_CTRL->VID || *(vu16*)(HIDHeap+0x12) != HID_CTRL->PID )
		{
			dbgprintf("HID:Config does not match device VID/PID\r\n");
			dbgprintf("HID:Config VID:%04X PID:%04X\r\n", HID_CTRL->VID, HID_CTRL->PID );

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

		if( Packet == (u8*)NULL )
		{
			if( HID_CTRL->Polltype )
			{
				Packet = (u8*)malloca(wMaxPacketSize, 32);
			} else if( HID_CTRL->Polltype == 0 ) {
				Packet = (u8*)malloca(128, 32);
			} else {
				dbgprintf("HID: %u is an invalid Polltype value\r\n", HID_CTRL->Polltype );
				return -4;
			}
		}

		if( HID_CTRL->DPAD > 1 )
		{
			dbgprintf("HID: %u is an invalid DPAD value\r\n", HID_CTRL->DPAD );
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
	
		HID_CTRL->StickX	= ConfigGetValue( Data, "StickX", 0 );
		HID_CTRL->StickY	= ConfigGetValue( Data, "StickY", 0 );
	
		HID_CTRL->CStickX	= ConfigGetValue( Data, "CStickX", 0 );
		HID_CTRL->CStickY	= ConfigGetValue( Data, "CStickY", 0 );
	
		HID_CTRL->LAnalog	= ConfigGetValue( Data, "LAnalog", 0 );
		HID_CTRL->RAnalog	= ConfigGetValue( Data, "RAnalog", 0 );

		dbgprintf("HID:Config file for VID:%04X PID:%04X loaded\r\n", HID_CTRL->VID, HID_CTRL->PID );
	} else {
		dbgprintf("HID:Failed to open config file:%u\r\n", ret );
		free(HIDHeap);
		return -2;
	}
	memset32(HID_Packet, 0, wMaxPacketSize | SS_DATA_LEN); //just to make sure
	free(HIDHeap);

	return HIDHandle;
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
unsigned char rawData[] =
{
    0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x27, 0x10, 0x00, 0x32, 
    0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 
} ;

void HIDPS3SetLED( u8 led )
{	
	memset32( req, 0, sizeof( req_args ) );
	
	char *buf = (char*)malloca( 64, 32 );
	memset32( buf, 0, 64 );

	memcpy( buf, rawData, sizeof(rawData) );

    buf[10] = ss_led_pattern[led];
	
	req->device_no				= DeviceID;
	req->interrupt.dLength		= 49;
	req->interrupt.endpoint		= 0x02;
	req->data					= buf;

	s32 ret = IOS_Ioctl( HIDHandle, /*InterruptMessageIN*/4, req, 32, 0, 0 );
	if( ret < 0 ) 
		dbgprintf("ES:IOS_Ioctl():%d\r\n", ret );
	
	free(buf);
}
void HIDPS3SetRumble( u8 duration_right, u8 power_right, u8 duration_left, u8 power_left)
{	
	memset32( req, 0, sizeof( req_args ) );
	
	char *buf = (char*)malloca( 64, 32 );
	memset32( buf, 0, 64 );

	memcpy( buf, rawData, 49 );
	
	buf[3] = power_left;
	buf[5] = power_right;
	
	req->device_no				= DeviceID;
	req->interrupt.dLength		= 49;
	req->interrupt.endpoint		= 0x02;
	req->data					= buf;

	
	s32 ret = IOS_Ioctl( HIDHandle, /*InterruptMessageIN*/4, req, 32, 0, 0 );
	if( ret < 0 )
		dbgprintf("ES:IOS_Ioctl():%d\r\n", ret );
}

u32 HIDRumbleLast = 0;
void HIDPS3Read()
{
	s32 ret;
	memset32( req, 0, sizeof( req_args ) );
	memset32( Packet, 0, SS_DATA_LEN );

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
	if( HIDRumbleLast != *(vu32*)(0x13003010) )
	{
		HIDRumble( *(vu32*)(0x13003010) );
		HIDRumbleLast = *(vu32*)(0x13003010);
	}
	memcpy(HID_Packet, Packet, SS_DATA_LEN);
	sync_after_write(HID_Packet, SS_DATA_LEN);
	return;
}
void HIDIRQRead()
{
	s32 ret;

	memset32( req, 0, sizeof( req_args ) );

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
		udelay(500);
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
	_sprintf( entryname, "\r\n%s=", EntryName );

	char *str = strstr( Data, entryname );
	if( str == (char*)NULL )
	{
		dbgprintf("Entry:\"%s\" not found!\r\n", EntryName );
		return 0;
	}

	str += strlen(entryname); // Skip '='

	if( Entry == 0 )
	{
		return atox(str);

	} else if ( Entry == 1 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL )
		{
			dbgprintf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip ,

		return atox(str);
	}

	return 0;
}
u32 HID_Run(void *arg)
{
	IOS_Close(HIDHandle);
	HIDHandle = IOS_Open("/dev/usb/hid", 0 );
	dbgprintf("HID_Run, waiting for signal\r\n");
	while(read32(0x13003004) == 0)
	{
		sync_before_read((void*)0x13003004, 0x20);
		mdelay(500);
	}
	dbgprintf("Starting HID Thread!\r\n");
	bool Polltype = HID_CTRL->Polltype;
	while(1)
	{
		if(Polltype)
			HIDIRQRead();
		else
			HIDPS3Read();
		mdelay(33);	// about 29 times a second
	}
	return 0;
}
