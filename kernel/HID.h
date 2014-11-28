#ifndef __HID_H__
#define __HID_H__

#include "global.h"
#include "syscalls.h"
#include "common.h"
#include "vsprintf.h"
#include "alloc.h"
#include "PS3Controller.h"

typedef struct Layout
{
	u32 Offset;
	u32 Mask;
} layout;

typedef struct StickLayout
{
	u32 	Offset;
	s8		DeadZone;
	u32		Radius;
} stickLayout;

typedef struct Controller
{
	u32 VID;
	u32 PID;
	u32 Polltype;
	u32 DPAD;
	u32 DPADMask;
	u32 DigitalLR;
	u32 MultiIn;
	u32 MultiInValue;

	layout Power;

	layout A;
	layout B;
	layout X;
	layout Y;
	layout ZL;
	layout Z;
	
	layout L;
	layout R;
	layout S;
	
	layout Left;
	layout Down;
	layout Right;
	layout Up;

	layout RightUp;
	layout DownRight;
	layout DownLeft;
	layout UpLeft;

	stickLayout StickX;
	stickLayout StickY;
	stickLayout CStickX;
	stickLayout CStickY;
	u32 LAnalog;
	u32 RAnalog;

} controller;

typedef struct {
	u8 padding[16]; // anything you want can go here
	s32 device_no;
	union {
		struct {
			u8 bmRequestType;
			u8 bmRequest;
			u16 wValue;
			u16 wIndex;
			u16 wLength;
		} control;
		struct {
			u32 endpoint;
			u32 dLength;
		} interrupt;
		struct {
			u8 bIndex;
		} string;
	};
	void *data; // virtual pointer, not physical!
} req_args; // 32 bytes

s32 HIDInit();
void HIDEnable();
void HIDClose();
void HIDUpdateRegisters();
void HIDGCInit( void );
void HIDPS3Init( void );
void HIDPS3Read( void );
void HIDIRQRead( void );
void HIDPS3SetLED( u8 led );
void HIDGCRumble( u32 Enable );
void HIDPS3Rumble( u32 Enable );
void HIDIRQRumble( u32 Enable );
void HIDCTRLRumble( u32 Enable );
u32 ConfigGetValue( char *Data, const char *EntryName, u32 Entry );
u32 ConfigGetDecValue( char *Data, const char *EntryName, u32 Entry );
void HIDPS3SetRumble( u8 duration_right, u8 power_right, u8 duration_left, u8 power_left);
u32 HID_Run(void *arg);

typedef void (*RumbleFunc)(u32 Enable);
extern RumbleFunc HIDRumble;

#endif
