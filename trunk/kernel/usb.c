/*-------------------------------------------------------------

usb.c -- USB lowlevel

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
tueidj

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/
/* Stripped down nintendont port by FIX94 */

/*  Note: There are 3 types of USB interfaces here, the early ones
 *  (V0: /dev/usb/oh0 and /dev/usb/oh1) and two later ones (V5: /dev/usb/ven
 *  and /dev/usb/hid) which are similar but have some small
 *  differences. There is also an earlier version of /dev/usb/hid (V4)
 *  found in IOSes 37,61,56,etc. and /dev/usb/msc found in IOS 57.
 *  These interfaces aren't implemented here and you may find some
 *  devices don't show up if you're running under those IOSes.
 */
#include "Config.h"
#include "usb.h"
#include "vsprintf.h"

#define USBV5_IOCTL_GETVERSION                   0 // should return 0x50001
#define USBV5_IOCTL_GETDEVICECHANGE              1
#define USBV5_IOCTL_SHUTDOWN                     2
#define USBV5_IOCTL_GETDEVPARAMS                 3
#define USBV5_IOCTL_ATTACHFINISH                 6
#define USBV5_IOCTL_SETALTERNATE                 7
#define USBV5_IOCTL_SUSPEND_RESUME              16
#define USBV5_IOCTL_CANCELENDPOINT              17
#define USBV5_IOCTL_CTRLMSG                     18
#define USBV5_IOCTL_INTRMSG                     19
#define USBV5_IOCTL_ISOMSG                      20
#define USBV5_IOCTL_BULKMSG                     21
#define USBV5_IOCTL_MSC_READWRITE_ASYNC         32 /* unimplemented */
#define USBV5_IOCTL_MSC_READ_ASYNC              33 /* unimplemented */
#define USBV5_IOCTL_MSC_WRITE_ASYNC             34 /* unimplemented */
#define USBV5_IOCTL_MSC_READWRITE               35 /* unimplemented */
#define USBV5_IOCTL_MSC_RESET                   36 /* unimplemented */

static const char __ven_path[] __attribute__((aligned(32))) = "/dev/usb/ven";
static s32 ven_fd = -1;

static s32 __usb_control_message(s32 device_id,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	struct _usb_msg msg ALIGNED(32); //use aligned stack instead of mem alloc

	if(((s32)rpData%32)!=0) return IPC_EINVAL;
	if(wLength && !rpData) return IPC_EINVAL;
	if(!wLength && rpData) return IPC_EINVAL;

	msg.fd = device_id;

	msg.ctrl.bmRequestType = bmRequestType;
	msg.ctrl.bmRequest = bmRequest;
	msg.ctrl.wValue = wValue;
	msg.ctrl.wIndex = wIndex;
	msg.ctrl.wLength = wLength;
	msg.ctrl.rpData = rpData;

	msg.vec[0].data = &msg;
	msg.vec[0].len = 64;
	msg.vec[1].data = rpData;
	msg.vec[1].len = wLength;

	u8 request_dir = !!(bmRequestType & USB_CTRLTYPE_DIR_DEVICE2HOST);
	return IOS_Ioctlv(ven_fd, USBV5_IOCTL_CTRLMSG, 2-request_dir, request_dir, msg.vec);
}

static inline s32 __usb_interrupt_bulk_message(s32 device_id,u8 ioctl,u8 bEndpoint,u16 wLength,void *rpData)
{
	struct _usb_msg msg ALIGNED(32); //use aligned stack instead of mem alloc

	if(((s32)rpData%32)!=0) return IPC_EINVAL;
	if(wLength && !rpData) return IPC_EINVAL;
	if(!wLength && rpData) return IPC_EINVAL;

	msg.fd = device_id;

	if (ioctl == USBV5_IOCTL_INTRMSG) {
		msg.intr.rpData = rpData;
		msg.intr.wLength = wLength;
		msg.intr.bEndpoint = bEndpoint;
	} else {
		msg.bulk.rpData = rpData;
		msg.bulk.wLength = wLength;
		msg.bulk.bEndpoint = bEndpoint;
	}

	msg.vec[0].data = &msg;
	msg.vec[0].len = 64;
	msg.vec[1].data = rpData;
	msg.vec[1].len = wLength;

	u8 endpoint_dir = !!(bEndpoint & USB_ENDPOINT_IN);
	return IOS_Ioctlv(ven_fd, ioctl, 2-endpoint_dir, endpoint_dir, msg.vec);
}

s32 USB_Initialize()
{
	if (ven_fd < 0)
		ven_fd = IOS_Open(__ven_path, IPC_OPEN_NONE);

	return IPC_OK;
}

s32 USB_Deinitialize()
{
	if (ven_fd >= 0) {
		IOS_Ioctl(ven_fd, USBV5_IOCTL_SHUTDOWN, NULL, 0, NULL, 0);
		IOS_Close(ven_fd);
		ven_fd = -1;
	}

	return IPC_OK;
}

s32 USB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV5_IOCTL_INTRMSG,bEndpoint,wLength,rpData);
}

s32 USB_WriteIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV5_IOCTL_INTRMSG,bEndpoint,wLength,rpData);
}

s32 USB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV5_IOCTL_BULKMSG,bEndpoint,wLength,rpData);
}

s32 USB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV5_IOCTL_BULKMSG,bEndpoint,wLength,rpData);
}

s32 USB_ReadCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData);
}

s32 USB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData);
}

s32 USB_ClearHalt(s32 device_id, u8 endpoint)
{
	s32 ret;

	s32 *buf = (s32*)malloca(32,32);
	if (buf==NULL) return IPC_ENOMEM;

	buf[0] = device_id;
	buf[2] = endpoint;
	ret = IOS_Ioctl(ven_fd, USBV5_IOCTL_CANCELENDPOINT, buf, 32, NULL, 0);
	free(buf);

	return ret;
}
