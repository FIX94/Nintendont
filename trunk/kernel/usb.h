#ifndef __USB_H__
#define __USB_H__

#define USB_MAXPATH						IPC_MAXPATH_LEN

#define USB_OK							0
#define USB_FAILED						1

#define USB_CLASS_HID					0x03
#define USB_SUBCLASS_BOOT				0x01
#define USB_PROTOCOL_KEYBOARD			0x01
#define USB_PROTOCOL_MOUSE				0x02

#define USB_REPTYPE_INPUT				0x01
#define USB_REPTYPE_OUTPUT				0x02
#define USB_REPTYPE_FEATURE				0x03

/* Descriptor types */
#define USB_DT_DEVICE					0x01
#define USB_DT_CONFIG					0x02
#define USB_DT_STRING					0x03
#define USB_DT_INTERFACE				0x04
#define USB_DT_ENDPOINT					0x05
#define USB_DT_DEVICE_QUALIFIER         0x06
#define USB_DT_OTHER_SPEED_CONFIG       0x07
#define USB_DT_INTERFACE_POWER          0x08
#define USB_DT_OTG                      0x09
#define USB_DT_DEBUG                    0x10
#define USB_DT_INTERFACE_ASSOCIATION    0x11
#define USB_DT_HID						0x21
#define USB_DT_REPORT					0x22
#define USB_DT_PHYSICAL					0x23
#define USB_DT_CLASS_SPECIFIC_INTERFACE 0x24
#define USB_DT_CLASS_SPECIFIC_ENDPOINT  0x25
#define USB_DT_HUB                      0x29

/* Standard requests */
#define USB_REQ_GETSTATUS				0x00
#define USB_REQ_CLEARFEATURE			0x01
#define USB_REQ_SETFEATURE				0x03
#define USB_REQ_SETADDRESS				0x05
#define USB_REQ_GETDESCRIPTOR			0x06
#define USB_REQ_SETDESCRIPTOR			0x07
#define USB_REQ_GETCONFIG				0x08
#define USB_REQ_SETCONFIG				0x09
#define USB_REQ_GETINTERFACE			0x0A
#define USB_REQ_SETINTERFACE			0x0B
#define USB_REQ_SYNCFRAME				0x0C

#define USB_REQ_GETREPORT				0x01
#define USB_REQ_GETIDLE					0x02
#define USB_REQ_GETPROTOCOL				0x03
#define USB_REQ_SETREPORT				0x09
#define USB_REQ_SETIDLE					0x0A
#define USB_REQ_SETPROTOCOL				0x0B

/* Descriptor sizes per descriptor type */
#define USB_DT_DEVICE_SIZE				18
#define USB_DT_CONFIG_SIZE				9
#define USB_DT_INTERFACE_SIZE			9
#define USB_DT_ENDPOINT_SIZE			7
#define USB_DT_ENDPOINT_AUDIO_SIZE		9	/* Audio extension */
#define USB_DT_HID_SIZE					9
#define USB_DT_HUB_NONVAR_SIZE			7

/* control message request type bitmask */
#define USB_CTRLTYPE_DIR_HOST2DEVICE	(0<<7)
#define USB_CTRLTYPE_DIR_DEVICE2HOST	(1<<7)
#define USB_CTRLTYPE_TYPE_STANDARD		(0<<5)
#define USB_CTRLTYPE_TYPE_CLASS			(1<<5)
#define USB_CTRLTYPE_TYPE_VENDOR		(2<<5)
#define USB_CTRLTYPE_TYPE_RESERVED		(3<<5)
#define USB_CTRLTYPE_REC_DEVICE			0
#define USB_CTRLTYPE_REC_INTERFACE		1
#define USB_CTRLTYPE_REC_ENDPOINT		2
#define USB_CTRLTYPE_REC_OTHER			3

#define USB_REQTYPE_INTERFACE_GET		(USB_CTRLTYPE_DIR_DEVICE2HOST|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)
#define USB_REQTYPE_INTERFACE_SET		(USB_CTRLTYPE_DIR_HOST2DEVICE|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)
#define USB_REQTYPE_ENDPOINT_GET		(USB_CTRLTYPE_DIR_DEVICE2HOST|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_ENDPOINT)
#define USB_REQTYPE_ENDPOINT_SET		(USB_CTRLTYPE_DIR_HOST2DEVICE|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_ENDPOINT)

#define USB_FEATURE_ENDPOINT_HALT		0

#define USB_ENDPOINT_INTERRUPT			0x03
#define USB_ENDPOINT_IN					0x80
#define USB_ENDPOINT_OUT				0x00

#define USB_OH0_DEVICE_ID				0x00000000				// for completion
#define USB_OH1_DEVICE_ID				0x00200000

#define USB_MAX_DEVICES					32

typedef struct _usb_device_entry {
	s32 device_id;
	u16 vid;
	u16 pid;
	u32 token;
} usb_device_entry;

struct _usb_msg {
	s32 fd;
	u32 heap_buffers;
	union {
		struct {
			u8 bmRequestType;
			u8 bmRequest;
			u16 wValue;
			u16 wIndex;
			u16 wLength;
			void *rpData;
		} ctrl;

		struct {
			void *rpData;
			u16 wLength;
			u8 pad[4];
			u8 bEndpoint;
		} bulk;

		struct {
			void *rpData;
			u16 wLength;
			u8 bEndpoint;
		} intr;

		struct {
			void *rpData;
			void *rpPacketSizes;
			u8 bPackets;
			u8 bEndpoint;
		} iso;

		struct {
			u16 pid;
			u16 vid;
		} notify;

		u8 class;
		u32 hid_intr_dir;

		u32 align_pad[6]; // pad to 32 bytes
	};
	ioctlv vec[7];
};

s32 USB_Initialize();
s32 USB_Deinitialize();

s32 USB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_ReadCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);

s32 USB_WriteIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);

s32 USB_ClearHalt(s32 fd, u8 endpointAddress);

#endif
