#ifndef __HID_H__
#define __HID_H__

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

typedef struct _usbdevdesc 
{
	u8  bLength;
	u8  bDescriptorType;
	u16 bcdUSB;

	u8  bDeviceClass;
	u8  bDeviceSubClass;
	u8  bDeviceProtocol;
	u8  bMaxPacketSize0;

	u16 idVendor;
	u16 idProduct;
	
	u16 bcdDevice;
	u8  iManufacturer;
	u8  iProduct;

	u8  iSerialNumber;
	u8  bNumConfigurations;
	u32 *configurations;
} __attribute__((packed)) usb_devdesc;


#define USB_REPTYPE_FEATURE                             0x03
#define USB_REPTYPE_OUTPUT                              0x02
#define USB_REPTYPE_INPUT                               0x01


#define USB_REQ_GETREPORT                               0x01
#define USB_REQ_GETDESCRIPTOR                   0x06
#define USB_REQ_SETREPORT                               0x09


/* control message request type bitmask */
#define USB_CTRLTYPE_DIR_HOST2DEVICE    (0<<7)
#define USB_CTRLTYPE_DIR_DEVICE2HOST    (1<<7)
#define USB_CTRLTYPE_TYPE_STANDARD              (0<<5)
#define USB_CTRLTYPE_TYPE_CLASS                 (1<<5)
#define USB_CTRLTYPE_TYPE_VENDOR                (2<<5)
#define USB_CTRLTYPE_TYPE_RESERVED              (3<<5)
#define USB_CTRLTYPE_REC_DEVICE                 0
#define USB_CTRLTYPE_REC_INTERFACE              1
#define USB_CTRLTYPE_REC_ENDPOINT               2
#define USB_CTRLTYPE_REC_OTHER                  3


#define USB_REQTYPE_INTERFACE_GET       (USB_CTRLTYPE_DIR_DEVICE2HOST|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)
#define USB_REQTYPE_INTERFACE_SET       (USB_CTRLTYPE_DIR_HOST2DEVICE|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)

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

#endif
