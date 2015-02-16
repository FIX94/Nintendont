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

static struct _usbv5_host* ven_host = NULL;

static s32 __find_device_on_host(struct _usbv5_host *host, s32 device_id)
{
	int i;
	if (host==NULL) return -1;

	for (i=0; host->attached_devices[i].device_id; i++) {
		if (host->attached_devices[i].device_id == device_id)
			return i;
	}

	return -1;
}

static s32 __usb_isochronous_message(s32 device_id,u8 bEndpoint,u8 bPackets,u16* rpPacketSizes,void* rpData)
{
	struct _usb_msg msg ALIGNED(32); //use aligned stack instead of mem alloc
	u16 wLength=0;
	u8 i;

	for (i=0; i<bPackets; i++)
		wLength += rpPacketSizes[i];

	if(wLength==0) return IPC_EINVAL;
	if(((u32)rpData%32)!=0) return IPC_EINVAL;
	if(((u32)rpPacketSizes%32)!=0) return IPC_EINVAL;
	if(bPackets==0) return IPC_EINVAL;
	if(rpPacketSizes==NULL || rpData==NULL) return IPC_EINVAL;

	msg.fd = device_id;

	msg.iso.rpData = rpData;
	msg.iso.rpPacketSizes = rpPacketSizes;
	msg.iso.bEndpoint = bEndpoint;
	msg.iso.bPackets = bPackets;

	msg.vec[0].data = &msg;
	msg.vec[0].len = 64;
	// block counts are used for both input and output
	msg.vec[1].data = msg.vec[3].data = rpPacketSizes;
	msg.vec[1].len = msg.vec[3].len = sizeof(u16)*bPackets;
	msg.vec[2].data = rpData;
	msg.vec[2].len = wLength;

	u8 endpoint_dir = !!(bEndpoint & USB_ENDPOINT_IN);
	return IOS_Ioctlv(ven_host->fd, USBV5_IOCTL_ISOMSG, 2-endpoint_dir, 2+endpoint_dir, msg.vec);
}

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
	return IOS_Ioctlv(ven_host->fd, USBV5_IOCTL_CTRLMSG, 2-request_dir, request_dir, msg.vec);
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

	if(bEndpoint & USB_ENDPOINT_IN) //host to device
		return IOS_Ioctlv(ven_host->fd, ioctl, 1, 1, msg.vec);
	//device to host
	s32 ret = IOS_Ioctlv(ven_host->fd, ioctl, 2, 0, msg.vec);
	return ret;
}

static inline s32 __usb_getdesc(s32 fd, u8 *buffer, u8 valuehi, u8 valuelo, u16 index, u16 size)
{
	u8 requestType = USB_CTRLTYPE_DIR_DEVICE2HOST;

	if (valuehi==USB_DT_HID || valuehi==USB_DT_REPORT || valuehi==USB_DT_PHYSICAL)
		requestType |= USB_CTRLTYPE_REC_INTERFACE;

	return __usb_control_message(fd, requestType, USB_REQ_GETDESCRIPTOR, (valuehi << 8) | valuelo, index, size, buffer);
}

static u32 __find_next_endpoint(u8 *buffer,s32 size,u8 align)
{
	u8 *ptr = buffer;

	while(size>2 && buffer[0]) { // abort if buffer[0]==0 to avoid getting stuck
		if(buffer[1]==USB_DT_ENDPOINT || buffer[1]==USB_DT_INTERFACE)
			break;

		size -= (buffer[0]+align)&~align;
		buffer += (buffer[0]+align)&~align;
	}

	return (buffer - ptr);
}

s32 USB_Initialize()
{
	if (ven_host==NULL) {
		s32 ven_fd = IOS_Open(__ven_path, IPC_OPEN_NONE);
		if (ven_fd>=0) {
			ven_host = (struct _usbv5_host*)malloca(sizeof(*ven_host),32);
			if (ven_host==NULL) {
				IOS_Close(ven_fd);
				return IPC_ENOMEM;
			}
			memset(ven_host, 0, sizeof(*ven_host));
			ven_host->fd = ven_fd;

			u32 *ven_ver = (u32*)malloca(0x20,32);
			if (ven_ver==NULL) goto mem_error;
			if (IOS_Ioctl(ven_fd, USBV5_IOCTL_GETVERSION, NULL, 0, ven_ver, 0x20) != 0 || ven_ver[0] != 0x50001)
			{
				// wrong ven version
				IOS_Close(ven_fd);
				free(ven_host);
				ven_host = NULL;
			}

			free(ven_ver);
		}
	}

	return IPC_OK;

mem_error:
	USB_Deinitialize();
	return IPC_ENOMEM;
}

s32 USB_Deinitialize()
{
	if (ven_host) {
		if (ven_host->fd>=0) {
			IOS_Ioctl(ven_host->fd, USBV5_IOCTL_SHUTDOWN, NULL, 0, NULL, 0);
			IOS_Close(ven_host->fd);
		}
		free(ven_host);
		ven_host = NULL;
	}

	return IPC_OK;
}

s32 USB_OpenDevice(s32 device_id,u16 vid,u16 pid,s32 *fd)
{
	s32 ret = USB_OK;
	char *devicepath = NULL;
	*fd = -1;

	if (device_id && device_id != USB_OH1_DEVICE_ID)
	{
		int i;

		i = __find_device_on_host(ven_host, device_id);
		if (i>=0)
		{
			USB_ResumeDevice(device_id);
			*fd = device_id;
			return 0;
		}
	}

	devicepath = malloca(USB_MAXPATH,32);
	if(devicepath==NULL) return IPC_ENOMEM;

	if (device_id==USB_OH1_DEVICE_ID)
		_sprintf(devicepath,"/dev/usb/oh1/%x/%x",vid,pid);
	else
		_sprintf(devicepath,"/dev/usb/oh0/%x/%x",vid,pid);

	*fd = IOS_Open(devicepath,0);
	if(*fd<0) ret = *fd;

	if (devicepath!=NULL) free(devicepath);
	return ret;
}

s32 USB_CloseDevice(s32 *fd)
{
	int i;
	struct _usbv5_host* host;
	s32 device_id = *fd;
	if (__find_device_on_host(ven_host, device_id)>=0)
		host = ven_host;
	else
		return IPC_EINVAL;

	for (i=0; i < USB_MAX_DEVICES; i++) {
		if (host->remove_cb[i].cb==NULL)
			continue;

		if (host->remove_cb[i].device_id==device_id) {
			host->remove_cb[i].cb(0, host->remove_cb[i].userdata);
			host->remove_cb[i].cb = NULL;
			break;
		}
	}
	//return USB_SuspendDevice(device_id);
	*fd = -1;

	return 0;
}

s32 USB_GetDescriptors(s32 device_id, usb_devdesc *udd)
{
	s32 retval = IPC_ENOMEM;
	u32 *io_buffer = NULL;
	u8 *buffer = NULL;
	usb_configurationdesc *ucd = NULL;
	usb_interfacedesc *uid = NULL;
	usb_endpointdesc *ued = NULL;
	u32 iConf, iEndpoint;
	s32 fd;
	u32 desc_out_size, desc_start_offset;

	if (__find_device_on_host(ven_host, device_id)>=0) {
		fd = ven_host->fd;
		desc_out_size = 0xC0;
		desc_start_offset = 20;
	} else
		return IPC_EINVAL;

	io_buffer = (u32*)malloca(0x20,32);
	buffer = (u8*)malloca(desc_out_size,32);
	if (io_buffer==NULL || buffer==NULL) goto free_bufs;

	io_buffer[0] = device_id;
	io_buffer[2] = 0;
	memset(buffer, 0, desc_out_size);
	retval = IOS_Ioctl(fd, USBV5_IOCTL_GETDEVPARAMS, io_buffer, 0x20, buffer, desc_out_size);
	if (retval==IPC_OK) {
		u8 *next = buffer+desc_start_offset;

		memcpy(udd, next, sizeof(*udd));
		udd->configurations = malloca(udd->bNumConfigurations* sizeof(*udd->configurations),32);
		if(udd->configurations == NULL)	goto free_bufs;
		memset32(udd->configurations,0,udd->bNumConfigurations* sizeof(*udd->configurations));

		next += (udd->bLength+3)&~3;
		for (iConf = 0; iConf < udd->bNumConfigurations; iConf++)
		{
			ucd = &udd->configurations[iConf];
			memcpy(ucd, next, USB_DT_CONFIG_SIZE);
			next += (USB_DT_CONFIG_SIZE+3)&~3;

			// ignore the actual value of bNumInterfaces; IOS presents each interface as a different device
			// alternate settings will not show up here, if you want them you must explicitly request
			// the interface descriptor
			if (ucd->bNumInterfaces==0)
				continue;

			ucd->bNumInterfaces = 1;
			ucd->interfaces = malloca(sizeof(*ucd->interfaces),32);
			if (ucd->interfaces == NULL) goto free_bufs;
			memset32(ucd->interfaces,0,sizeof(*ucd->interfaces));

			uid = ucd->interfaces;
			memcpy(uid, next, USB_DT_INTERFACE_SIZE);
			next += (uid->bLength+3)&~3;

			/* This skips vendor and class specific descriptors */
			uid->extra_size = __find_next_endpoint(next, buffer+desc_out_size-next, 3);
			if(uid->extra_size>0)
			{
				uid->extra = malloc(uid->extra_size);
				if(uid->extra == NULL)
					goto free_bufs;
				memcpy(uid->extra, next, uid->extra_size);
				// already rounded
				next += uid->extra_size;
			}

			if (uid->bNumEndpoints) {
				uid->endpoints = malloca(uid->bNumEndpoints * sizeof(*uid->endpoints),32);
				if (uid->endpoints == NULL)	goto free_bufs;
				memset32(uid->endpoints,0,uid->bNumEndpoints * sizeof(*uid->endpoints));

				for(iEndpoint = 0; iEndpoint < uid->bNumEndpoints; iEndpoint++)
				{
					ued = &uid->endpoints[iEndpoint];
					memcpy(ued, next, USB_DT_ENDPOINT_SIZE);
					next += (ued->bLength+3)&~3;
				}
			}

		}

		retval = IPC_OK;
	}

free_bufs:
	if (io_buffer!=NULL)
		free(io_buffer);
	if (buffer!=NULL)
		free(buffer);
	if (retval<0)
		USB_FreeDescriptors(udd);
	return retval;
}

s32 USB_ReadIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData)
{
	return __usb_isochronous_message(fd,bEndpoint,bPackets,rpPacketSizes,rpData);
}

s32 USB_WriteIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData)
{
	return __usb_isochronous_message(fd,bEndpoint,bPackets,rpPacketSizes,rpData);
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

static s32 USBV5_SuspendResume(s32 device_id, s32 resumed)
{
	s32 ret;
	s32 fd;

	if (__find_device_on_host(ven_host, device_id)>=0)
		fd = ven_host->fd;
	else
		return IPC_ENOENT;

	s32 *buf = (s32*)malloca(32,32);
	if (buf==NULL) return IPC_ENOMEM;

	buf[0] = device_id;
	buf[2] = resumed;
	ret = IOS_Ioctl(fd, USBV5_IOCTL_SUSPEND_RESUME, buf, 32, NULL, 0);
	free(buf);

	return ret;
}

s32 USB_SuspendDevice(s32 fd)
{
	return USBV5_SuspendResume(fd, 0);
}

s32 USB_ResumeDevice(s32 fd)
{
	return USBV5_SuspendResume(fd, 1);
}

s32 USB_GetDeviceList(usb_device_entry *descr_buffer,u8 num_descr,u8 interface_class,u8 *cnt_descr)
{
	int i;
	u8 cntdevs=0;

	// for ven_host, we can only exclude usb_hid class devices
	if (interface_class != USB_CLASS_HID && ven_host) {
		//since its not actually a new IOS we cant actually ask for the device changes
		sync_before_read((void*)0x132C1000, sizeof(usb_device_entry)*USB_MAX_DEVICES);
		memcpy(ven_host->attached_devices, (void*)0x132C1000, sizeof(usb_device_entry)*USB_MAX_DEVICES);
		i=0;
		while (cntdevs<num_descr && ven_host->attached_devices[i].device_id) {
			descr_buffer[cntdevs++] = ven_host->attached_devices[i++];
			if (i>=32) break;
		}
	}

	if (cnt_descr) *cnt_descr = cntdevs;

	return IPC_OK;
}

s32 USB_SetConfiguration(s32 fd, u8 configuration)
{
	return __usb_control_message(fd, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_DEVICE), USB_REQ_SETCONFIG, configuration, 0, 0, NULL);
}

s32 USB_GetConfiguration(s32 fd, u8 *configuration)
{
	u8 *_configuration;
	s32 retval;

	_configuration = malloca(32,32);
	if(_configuration == NULL)
		return IPC_ENOMEM;

	retval = __usb_control_message(fd, (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_DEVICE), USB_REQ_GETCONFIG, 0, 0, 1, _configuration);
	if(retval >= 0)
		*configuration = *_configuration;
	free(_configuration);

	return retval;
}

s32 USB_SetAlternativeInterface(s32 fd, u8 interface, u8 alternateSetting)
{
	return __usb_control_message(fd, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_INTERFACE), USB_REQ_SETINTERFACE, alternateSetting, interface, 0, NULL);
}

s32 USB_ClearHalt(s32 device_id, u8 endpoint)
{
	s32 ret;
	s32 fd;

	if (__find_device_on_host(ven_host, device_id)>=0)
		fd = ven_host->fd;
	else
		return IPC_ENOENT;

	s32 *buf = (s32*)malloca(32,32);
	if (buf==NULL) return IPC_ENOMEM;

	buf[0] = device_id;
	buf[2] = endpoint;
	ret = IOS_Ioctl(fd, USBV5_IOCTL_CANCELENDPOINT, buf, 32, NULL, 0);
	free(buf);

	return ret;
}

void USB_FreeDescriptors(usb_devdesc *udd)
{
	int iConf, iInterface;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	if(udd->configurations != NULL)
	{
		for(iConf = 0; iConf < udd->bNumConfigurations; iConf++)
		{
			ucd = &udd->configurations[iConf];
			if(ucd->interfaces != NULL)
			{
				for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
				{
					uid = ucd->interfaces+iInterface;
					free(uid->endpoints);
					free(uid->extra);
				}
				free(ucd->interfaces);
			}
		}
		free(udd->configurations);
	}
}
