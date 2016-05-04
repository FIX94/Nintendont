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
#include "usb.h"
#include "HID_controllers.h"
#ifndef DEBUG_HID
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

#define GetDeviceChange 1
#define GetDeviceParameters 3
#define AttachFinish 6
#define ResumeDevice 16
#define ControlMessage 18
#define InterruptMessage 19

static u8 ss_led_pattern[8] = {0x0, 0x02, 0x04, 0x08, 0x10, 0x12, 0x14, 0x18};

s32 HIDHandle = -1;
u32 PS3LedSet = 0;
u32 DeviceID  = 0;
u32 bEndpointAddress = 0;
u32 wMaxPacketSize = 0;
u32 MemPacketSize = 0;
u8 *Packet = (u8*)NULL;

u32 RumbleType = 0;
u32 RumbleEnabled = 0;
u32 bEndpointAddressOut = 0;
u8 *RawRumbleDataOn = NULL;
u8 *RawRumbleDataOff = NULL;
u32 RawRumbleDataLen = 0;
u32 RumbleTransferLen = 0;
u32 RumbleTransfers = 0;
unsigned char rawData[] =
{
    0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x27, 0x10, 0x00, 0x32, 
    0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 
} ;

struct _usb_msg readreq ALIGNED(32);
struct _usb_msg writereq ALIGNED(32);
u8 *ps3buf = (u8*)NULL;
u8 *gcbuf = (u8*)NULL;

typedef void (*HIDReadFunc)();
HIDReadFunc HIDRead = NULL;

typedef void (*RumbleFunc)(u32 Enable);
RumbleFunc HIDRumble = NULL;

static usb_device_entry AttachedDevices[32] ALIGNED(32);

static struct ipcmessage *hidreadmsg = NULL, *hidchangemsg = NULL;
static u32 HIDRead_Thread = 0, HIDChange_Thread = 0;
u8 *hidreadheap = NULL, *hidchangeheap = NULL;
s32 hidreadqueue = -1, hidchangequeue = -1;
vu32 hidread = 0, hidchange = 0;
static u32 HIDReadAlarm();
static u32 HIDChangeAlarm();
static s32 HIDInterruptMessage(u8 *Data, u32 Length, u32 Endpoint, s32 asyncqueue, struct ipcmessage *asyncmsg);
static s32 HIDControlMessage(u8 *Data, u32 Length, u32 RequestType, u32 Request, u32 Value, s32 asyncqueue, struct ipcmessage *asyncmsg);
extern char __hid_read_stack_addr, __hid_read_stack_size;
extern char __hid_change_stack_addr, __hid_change_stack_size;

#define HID_STATUS 0x13003440
#define HID_CHANGE HID_STATUS+4
#define HID_CFG_SIZE HID_STATUS+8
#define HID_CFG_FILE 0x13003460

void HIDInit( void )
{
	HIDHandle = IOS_Open("/dev/usb/hid", 0 );
	if(HIDHandle < 0) return; //should never happen

	ps3buf = (u8*)malloca( 64, 32 );
	gcbuf = (u8*)malloca( 32,32 );

	hidreadheap = (u8*)malloca(32,32);
	hidreadqueue = mqueue_create(hidreadheap, 1);
	hidreadmsg = (struct ipcmessage*)malloca(sizeof(struct ipcmessage), 32);
	HIDRead_Thread = thread_create(HIDReadAlarm, NULL, ((u32*)&__hid_read_stack_addr), ((u32)(&__hid_read_stack_size)) / sizeof(u32), 0x78, 1);
	thread_continue(HIDRead_Thread);

	hidchangeheap = (u8*)malloca(32,32);
	hidchangequeue = mqueue_create(hidchangeheap, 1);
	hidchangemsg = (struct ipcmessage*)malloca(sizeof(struct ipcmessage), 32);
	HIDChange_Thread = thread_create(HIDChangeAlarm, NULL, ((u32*)&__hid_change_stack_addr), ((u32)(&__hid_change_stack_size)) / sizeof(u32), 0x78, 1);
	thread_continue(HIDChange_Thread);

	memset32(AttachedDevices, 0, sizeof(usb_device_entry)*32);
	IOS_IoctlAsync(HIDHandle, GetDeviceChange, NULL, 0, VirtualToPhysical(AttachedDevices), 0x180, hidchangequeue, hidchangemsg);

	memset32((void*)HID_STATUS, 0, 0x20);
	sync_after_write((void*)HID_STATUS, 0x20);

	mdelay(100);
}

s32 HIDOpen( u32 LoaderRequest )
{
	s32 ret = -1;
	dbgprintf("HIDOpen()\r\n");

	memset32(&readreq, 0, sizeof(struct _usb_msg));
	memset32(&writereq, 0, sizeof(struct _usb_msg));

	memset32(ps3buf, 0, 64);
	memcpy(ps3buf, rawData, sizeof(rawData));

	memset32(gcbuf, 0, 32);
	gcbuf[0] = 0x13;

	HIDRead = NULL;
	HIDRumble = NULL;

	RumbleEnabled = 0;

	memset32((void*)HID_STATUS, 0, 0x20);
	sync_after_write((void*)HID_STATUS, 0x20);
	//BootStatusError(8, 1);

	u32 i;
	u32 DeviceVID = 0, DevicePID = 0;
	for(i = 0; i < 32; ++i)
	{
		if(AttachedDevices[i].vid != 0)
		{
			DeviceID = AttachedDevices[i].device_id;
			DeviceVID = AttachedDevices[i].vid;
			DevicePID = AttachedDevices[i].pid;
			break;
		}
	}

	if( DeviceVID == 0 )
	{
		dbgprintf("HID:No controller connected!\r\n");
		return -1;
	}

	dbgprintf("HID:DeviceID:%u\r\n", DeviceID );
	dbgprintf("HID:VID:%04X PID:%04X\r\n", DeviceVID, DevicePID );

	s32 *io_buffer = (s32*)malloca(0x20, 32);
	memset32(io_buffer, 0, 0x20);
	io_buffer[0] = DeviceID;
	io_buffer[2] = 1; //resume device
	IOS_Ioctl(HIDHandle, ResumeDevice, io_buffer, 0x20, NULL, 0);

	u8 *HIDHeap = (u8*)malloca(0x60,32);
	memset32(HIDHeap, 0, 0x60);

	memset32(io_buffer, 0, 0x20);
	io_buffer[0] = DeviceID;
	io_buffer[2] = 0;
	IOS_Ioctl(HIDHandle, GetDeviceParameters, io_buffer, 0x20, HIDHeap, 0x60);
	free(io_buffer);

	BootStatusError(8, 0);

	u32 Offset = 36;

	u32 DeviceDescLength    = *(vu8*)(HIDHeap+Offset);
	Offset += (DeviceDescLength+3)&(~3);

	u32 ConfigurationLength = *(vu8*)(HIDHeap+Offset);
	Offset += (ConfigurationLength+3)&(~3);

	u32 InterfaceDescLength = *(vu8*)(HIDHeap+Offset);
	Offset += (InterfaceDescLength+3)&(~3);

	u32 EndpointDescLengthO = *(vu8*)(HIDHeap+Offset);

	bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);

	if( (bEndpointAddress & 0xF0) != 0x80 )
	{
		bEndpointAddressOut = bEndpointAddress;
		Offset += (EndpointDescLengthO+3)&(~3);
	}
	bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);
	wMaxPacketSize   = *(vu16*)(HIDHeap+Offset+4);

	dbgprintf("HID:bEndpointAddress:%02X\r\n", bEndpointAddress );
	dbgprintf("HID:wMaxPacketSize  :%u\r\n", wMaxPacketSize );

	free(HIDHeap);

	if( DeviceVID == 0x054c && DevicePID == 0x0268 )
	{
		dbgprintf("HID:PS3 Dualshock Controller detected\r\n");
		MemPacketSize = SS_DATA_LEN;
		HIDPS3Init();
		RumbleEnabled = 1;
		HIDPS3SetRumble( 0, 0, 0, 0 );
	}
	else if( DeviceVID == 0x057e && DevicePID == 0x0337 )
		HIDGCInit();

//Load controller config
	char *Data = NULL;
	if(LoaderRequest)
	{
		dbgprintf("Sending controller.ini request\r\n");
		memset32((void*)HID_STATUS, 0, 0x20);
		write32(HID_CHANGE, DeviceVID);
		write32(HID_CFG_SIZE, DevicePID);
		sync_after_write((void*)HID_STATUS, 0x20);
		while(1)
		{
			sync_before_read((void*)HID_STATUS, 0x20);
			if(read32(HID_CHANGE) == 0) break;
			mdelay(10);
		}
		u32 cfgsize = read32(HID_CFG_SIZE);
		if(cfgsize == 0)
			dbgprintf("HID:No controller config found!\r\n");
		else
		{
			Data = malloc(cfgsize+1);
			if(Data)
			{
				sync_before_read((void*)HID_CFG_FILE, cfgsize);
				memcpy(Data, (void*)HID_CFG_FILE, cfgsize);
				Data[cfgsize] = 0x00;	//null terminate the file
			}
		}
	}
	else
	{
		FIL f;
		u32 read;
		char directory[28];
		_sprintf(directory, "/controllers/%04X_%04X.ini", DeviceVID, DevicePID);
		dbgprintf("Preferred controller.ini file: %s\r\n", directory);
		
		ret = f_open_char( &f, directory, FA_OPEN_EXISTING|FA_READ);
		if(ret != FR_OK)
			ret = f_open_char( &f, "/controller.ini", FA_OPEN_EXISTING|FA_READ);
		else
			dbgprintf("%s was used\r\n", directory);
		if(ret != FR_OK)
			ret = f_open_char(&f, "/controller.ini.ini", FA_OPEN_EXISTING | FA_READ); // too many people don't read the instructions for windows
		if(ret != FR_OK)
			dbgprintf("HID:Failed to open config file:%u\r\n", ret );
		else
		{
			Data = (char*)malloc( f.obj.objsize + 1 );
			if(Data)
			{
				f_read( &f, Data, f.obj.objsize, &read );
				Data[f.obj.objsize] = 0x00;	//null terminate the file
			}
			f_close(&f);
		}
	}
	if(Data != NULL) //initial check
	{
		HID_CTRL->VID = ConfigGetValue( Data, "VID", 0 );
		HID_CTRL->PID = ConfigGetValue( Data, "PID", 0 );

		if( DeviceVID != HID_CTRL->VID || DevicePID != HID_CTRL->PID )
		{
			dbgprintf("HID:Config does not match device VID/PID\r\n");
			dbgprintf("HID:Config VID:%04X PID:%04X\r\n", HID_CTRL->VID, HID_CTRL->PID );
			free(Data);
			Data = NULL;
		}
	}
	if(Data == NULL)
	{
		controller *c = NULL;
		u32 i;
		for(i = 0; i < sizeof(DefControllers) / sizeof(controller); ++i)
		{
			if(DefControllers[i].VID == DeviceVID && DefControllers[i].PID == DevicePID)
			{
				c = &DefControllers[i];
				dbgprintf("HID:Using Internal Controller Settings\r\n");
				break;
			}
		}
		if(c == NULL)
		{
			dbgprintf("HID:No Configs Found!\r\n");
			return -2;
		}
		memcpy(HID_CTRL, c, sizeof(controller));
		for(i = 0; i < sizeof(DefRumble) / sizeof(rumble); ++i)
		{
			if(DefRumble[i].VID == DeviceVID && DefRumble[i].PID == DevicePID)
			{
				RawRumbleDataLen = DefRumble[i].RumbleDataLen;
				if(RawRumbleDataLen > 0)
				{
					dbgprintf("HID:Using Internal Rumble Settings\r\n");
					RumbleEnabled = 1;
					u32 DataAligned = (RawRumbleDataLen+31) & (~31);

					if(RawRumbleDataOn != NULL) free(RawRumbleDataOn);
					RawRumbleDataOn = (u8*)malloca(DataAligned, 32);
					memset32(RawRumbleDataOn, 0, DataAligned);
					memcpy(RawRumbleDataOn, DefRumble[i].RumbleDataOn, RawRumbleDataLen);

					if(RawRumbleDataOff != NULL) free(RawRumbleDataOff);
					RawRumbleDataOff = (u8*)malloca(DataAligned, 32);
					memset32(RawRumbleDataOff, 0, DataAligned);
					memcpy(RawRumbleDataOff, DefRumble[i].RumbleDataOff, RawRumbleDataLen);

					RumbleType = DefRumble[i].RumbleType;
					RumbleTransferLen = DefRumble[i].RumbleTransferLen;
					RumbleTransfers = DefRumble[i].RumbleTransfers;
				}
				break;
			}
		}
	}
	else
	{
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

		if( HID_CTRL->DPAD > 1 )
		{
			dbgprintf("HID: %u is an invalid DPAD value\r\n", HID_CTRL->DPAD );
			free(Data);
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

		HID_CTRL->ZL.Offset	= ConfigGetValue( Data, "ZL", 0 );
		HID_CTRL->ZL.Mask	= ConfigGetValue( Data, "ZL", 1 );

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

		if( HID_CTRL->DPAD  &&	//DPAD == 1 and all offsets the same
			HID_CTRL->Left.Offset == HID_CTRL->Down.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->Right.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->Up.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->RightUp.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->DownRight.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->DownLeft.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->UpLeft.Offset )
		{
			HID_CTRL->DPADMask = HID_CTRL->Left.Mask | HID_CTRL->Down.Mask | HID_CTRL->Right.Mask | HID_CTRL->Up.Mask
				| HID_CTRL->RightUp.Mask | HID_CTRL->DownRight.Mask | HID_CTRL->DownLeft.Mask | HID_CTRL->UpLeft.Mask;	//mask is all the used bits ored togather
			if ((HID_CTRL->DPADMask & 0xF0) == 0)	//if hi nibble isnt used
				HID_CTRL->DPADMask = 0x0F;			//use all bits in low nibble
			if ((HID_CTRL->DPADMask & 0x0F) == 0)	//if low nibble isnt used
				HID_CTRL->DPADMask = 0xF0;			//use all bits in hi nibble
		}
		else
			HID_CTRL->DPADMask = 0xFFFF;	//check all the bits

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

		if(ConfigGetValue( Data, "Rumble", 0 ))
		{
			RawRumbleDataLen = ConfigGetValue( Data, "RumbleDataLen", 0 );
			if(RawRumbleDataLen > 0)
			{
				RumbleEnabled = 1;
				u32 DataAligned = (RawRumbleDataLen+31) & (~31);

				if(RawRumbleDataOn != NULL) free(RawRumbleDataOn);
				RawRumbleDataOn = (u8*)malloca(DataAligned, 32);
				memset32(RawRumbleDataOn, 0, DataAligned);
				ConfigGetValue( Data, "RumbleDataOn", 3 );

				if(RawRumbleDataOff != NULL) free(RawRumbleDataOff);
				RawRumbleDataOff = (u8*)malloca(DataAligned, 32);
				memset32(RawRumbleDataOff, 0, DataAligned);
				ConfigGetValue( Data, "RumbleDataOff", 4 );

				RumbleType = ConfigGetValue( Data, "RumbleType", 0 );
				RumbleTransferLen = ConfigGetValue( Data, "RumbleTransferLen", 0 );
				RumbleTransfers = ConfigGetValue( Data, "RumbleTransfers", 0 );
			}
		}
		free(Data);

		dbgprintf("HID:Config file for VID:%04X PID:%04X loaded\r\n", HID_CTRL->VID, HID_CTRL->PID );
	}

	if( HID_CTRL->Polltype == 0 )
		MemPacketSize = 128;
	else
		MemPacketSize = wMaxPacketSize;

	if(Packet != NULL) free(Packet);
	Packet = (u8*)malloca(MemPacketSize, 32);
	memset32(Packet, 0, MemPacketSize);
	sync_after_write(Packet, MemPacketSize);

	memset32(HID_Packet, 0, MemPacketSize);
	sync_after_write(HID_Packet, MemPacketSize);

	bool Polltype = HID_CTRL->Polltype;
	if((HID_CTRL->VID == 0x057E) && (HID_CTRL->PID == 0x0337))
	{
		HIDRumble = HIDGCRumble;
		RumbleEnabled = true;
	}
	else if(RumbleEnabled)
	{
		if(Polltype)
		{
			if(RumbleType)
				HIDRumble = HIDIRQRumble;
			else
				HIDRumble = HIDCTRLRumble;
		}
		else
			HIDRumble = HIDPS3Rumble;
	}

	if(Polltype)
	{
		HIDInterruptMessage(Packet, wMaxPacketSize, bEndpointAddress, hidreadqueue, hidreadmsg);
		HIDRead = HIDIRQRead;
	}
	else
	{
		HIDControlMessage(Packet, SS_DATA_LEN, USB_REQTYPE_INTERFACE_GET,
			USB_REQ_GETREPORT, (USB_REPTYPE_INPUT<<8) | 0x1, hidreadqueue, hidreadmsg);
		HIDRead = HIDPS3Read;
	}

	memset32((void*)HID_STATUS, 0, 0x20);
	write32(HID_STATUS, 1);
	sync_after_write((void*)HID_STATUS, 0x20);

	return 0;
}

void HIDClose()
{
	IOS_Close(HIDHandle);
	HIDHandle = -1;
}

static u32 HIDReadAlarm()
{
	struct ipcmessage *msg = NULL;
	while(1)
	{
		mqueue_recv(hidreadqueue, &msg, 0);
		mqueue_ack(msg, 0);
		hidread = 1;
	}
	return 0;
}

static u32 HIDChangeAlarm()
{
	struct ipcmessage *msg = NULL;
	while(1)
	{
		mqueue_recv(hidchangequeue, &msg, 0);
		mqueue_ack(msg, 0);
		hidchange = 1;
	}
	return 0;
}

static s32 HIDControlMessage(u8 *Data, u32 Length, u32 RequestType, u32 Request, u32 Value, s32 asyncqueue, struct ipcmessage *asyncmsg)
{
	u8 request_dir = !!(RequestType & USB_CTRLTYPE_DIR_DEVICE2HOST);

	struct _usb_msg *msg = VirtualToPhysical(request_dir ? &readreq : &writereq);
	msg->fd = DeviceID;

	msg->ctrl.bmRequestType = RequestType;
	msg->ctrl.bmRequest = Request;
	msg->ctrl.wValue = Value;
	msg->ctrl.wIndex = 0;
	msg->ctrl.wLength = Length;
	msg->ctrl.rpData = Data;

	msg->vec[0].data = msg;
	msg->vec[0].len = 64;
	msg->vec[1].data = Data;
	msg->vec[1].len = Length;

	if(asyncmsg != NULL)
		return IOS_IoctlvAsync(HIDHandle, ControlMessage, 2-request_dir, request_dir, msg->vec, asyncqueue, asyncmsg);
	return IOS_Ioctlv(HIDHandle, ControlMessage, 2-request_dir, request_dir, msg->vec);
}

static s32 HIDInterruptMessage(u8 *Data, u32 Length, u32 Endpoint, s32 asyncqueue, struct ipcmessage *asyncmsg)
{
	u8 endpoint_dir = !!(Endpoint & USB_ENDPOINT_IN);

	struct _usb_msg *msg = VirtualToPhysical(endpoint_dir ? &readreq : &writereq);
	msg->hid_intr_dir = !endpoint_dir;
	msg->fd = DeviceID;

	msg->vec[0].data = msg;
	msg->vec[0].len = 64;
	msg->vec[1].data = Data;
	msg->vec[1].len = Length;

	if(asyncmsg != NULL)
		return IOS_IoctlvAsync(HIDHandle, InterruptMessage, 2-endpoint_dir, endpoint_dir, msg->vec, asyncqueue, asyncmsg);
	return IOS_Ioctlv(HIDHandle, InterruptMessage, 2-endpoint_dir, endpoint_dir, msg->vec);
}
void HIDGCInit()
{
	s32 ret = HIDInterruptMessage(gcbuf, 1, bEndpointAddressOut, 0, NULL);
	if( ret < 0 )
	{
		dbgprintf("HID:HIDGCInit:IOS_Ioctl( %u, %u, %p, %u, %u, %u):%d\r\n", HIDHandle, 2, writereq, 32, 0, 0, ret );
		BootStatusError(-8, -7);
		mdelay(4000);
		Shutdown();
	}
}
void HIDPS3Init()
{
	u8 *buf = (u8*)malloca( 0x20, 32 );
	memset32( buf, 0, 0x20 );
	s32 ret = HIDControlMessage(buf, 17, USB_REQTYPE_INTERFACE_GET,
			USB_REQ_GETREPORT, (USB_REPTYPE_FEATURE<<8) | 0xf2, 0, NULL);
	if( ret < 0 )
	{
		dbgprintf("HID:HIDPS3Init:IOS_Ioctl( %u, %u, %p, %u, %u, %u):%d\r\n", HIDHandle, 2, readreq, 32, 0, 0, ret );
		BootStatusError(-8, -6);
		mdelay(4000);
		Shutdown();
	}
	free(buf);
}
void HIDPS3SetLED( u8 led )
{
	ps3buf[10] = ss_led_pattern[led];
	sync_after_write(ps3buf, 64);

	s32 ret = HIDInterruptMessage(ps3buf, sizeof(rawData), 0x02, 0, NULL);
	if( ret < 0 ) 
		dbgprintf("ES:IOS_Ioctl():%d\r\n", ret );
}
void HIDPS3SetRumble( u8 duration_right, u8 power_right, u8 duration_left, u8 power_left)
{
	ps3buf[3] = power_left;
	ps3buf[5] = power_right;
	sync_after_write(ps3buf, 64);

	s32 ret = HIDInterruptMessage(ps3buf, sizeof(rawData), 0x02, 0, NULL);
	if( ret < 0 )
		dbgprintf("ES:IOS_Ioctl():%d\r\n", ret );
}

vu32 HIDRumbleCurrent = 0, HIDRumbleLast = 0;
vu32 MotorCommand = 0x13002700;
void HIDPS3Read()
{
	if( !PS3LedSet && Packet[4] )
	{
		HIDPS3SetLED(1);
		PS3LedSet = 1;
	}
	memcpy(HID_Packet, Packet, SS_DATA_LEN);
	sync_after_write(HID_Packet, SS_DATA_LEN);

	HIDControlMessage(Packet, SS_DATA_LEN, USB_REQTYPE_INTERFACE_GET,
			USB_REQ_GETREPORT, (USB_REPTYPE_INPUT<<8) | 0x1, hidreadqueue, hidreadmsg);
}
void HIDGCRumble(u32 input)
{
	gcbuf[0] = 0x11;
	gcbuf[1] = input & 1;
	gcbuf[2] = (input >> 1) & 1;
	gcbuf[3] = (input >> 2) & 1;
	gcbuf[4] = (input >> 3) & 1;

	HIDInterruptMessage(gcbuf, 5, bEndpointAddressOut, 0, NULL);
}

void HIDIRQRumble(u32 Enable)
{
	u8 *buf = (Enable == 1) ? RawRumbleDataOn : RawRumbleDataOff;
	u32 i = 0;
irqrumblerepeat:
	HIDInterruptMessage(buf, RumbleTransferLen, bEndpointAddressOut, 0, NULL);
	i++;
	if(i < RumbleTransfers)
	{
		buf += RumbleTransferLen;
		goto irqrumblerepeat;
	}
}

void HIDCTRLRumble(u32 Enable)
{
	u8 *buf = (Enable == 1) ? RawRumbleDataOn : RawRumbleDataOff;
	u32 i = 0;
ctrlrumblerepeat:
	i++;
	HIDControlMessage(buf, RumbleTransferLen, USB_REQTYPE_INTERFACE_SET,
			USB_REQ_SETREPORT, (USB_REPTYPE_OUTPUT<<8) | 0x1, 0, NULL);
	if(i < RumbleTransfers)
	{
		buf += RumbleTransferLen;
		goto ctrlrumblerepeat;
	}
}

void HIDIRQRead()
{
	switch( HID_CTRL->MultiIn )
	{
		default:
		case 0:	// MultiIn disabled
		case 3: // multiple controllers from a single adapter all controllers in 1 message
			break;
		case 1:	// match single controller filter on the first byte
			if (Packet[0] != HID_CTRL->MultiInValue)
				goto dohidirqread;
			break;
		case 2: // multiple controllers from a single adapter first byte contains controller number
			if ((Packet[0] < HID_CTRL->MultiInValue) || (Packet[0] > NIN_CFG_MAXPAD))
				goto dohidirqread;
			break;
	}
	memcpy(HID_Packet, Packet, wMaxPacketSize);
	sync_after_write(HID_Packet, wMaxPacketSize);
dohidirqread:
	HIDInterruptMessage(Packet, wMaxPacketSize, bEndpointAddress, hidreadqueue, hidreadmsg);
}

void HIDPS3Rumble( u32 Enable )
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
	} else if ( Entry == 3 ) {
		u32 i;
		for(i = 0; i < RawRumbleDataLen; ++i)
		{
			RawRumbleDataOn[i] = atox(str);
			str = strstr( str, "," )+1;
		}
	} else if ( Entry == 4 ) {
		u32 i;
		for(i = 0; i < RawRumbleDataLen; ++i)
		{
			RawRumbleDataOff[i] = atox(str);
			str = strstr( str, "," )+1;
		}
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

void HIDUpdateRegisters(u32 LoaderRequest)
{
	if(hidchange == 1)
	{
		//If you dont do that it wont update anymore
		IOS_Ioctl(HIDHandle, AttachFinish, NULL, 0, NULL, 0);
		mdelay(10);
		hidread = 0;
		HIDOpen(LoaderRequest);
		hidchange = 0;
		memset32(AttachedDevices, 0, sizeof(usb_device_entry)*32);
		IOS_IoctlAsync(HIDHandle, GetDeviceChange, NULL, 0, VirtualToPhysical(AttachedDevices), 0x180, hidchangequeue, hidchangemsg);
	}
	if(hidread == 1)
	{
		hidread = 0;
		if(HIDRead) HIDRead();
	}
	if(RumbleEnabled)
	{
		//sync_before_read((void*)MotorCommand,0x20);
		HIDRumbleCurrent = read32(MotorCommand);
		if( HIDRumbleLast != HIDRumbleCurrent )
		{
			if(HIDRumble) HIDRumble( HIDRumbleCurrent );
			HIDRumbleLast = HIDRumbleCurrent;
		}
	}
}
