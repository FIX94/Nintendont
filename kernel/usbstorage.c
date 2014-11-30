/*-------------------------------------------------------------

usbstorage.c -- Bulk-only USB mass storage support

Copyright (C) 2008
Sven Peter (svpe) <svpe@gmx.net>
Copyright (C) 2009-2010
tueidj, rodries, Tantric

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
#include "Config.h"
#include "debug.h"
#include "usbstorage.h"
#include "usb.h"

#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

#define	HEAP_SIZE					(18*1024)
#define	TAG_START					0x0BADC0DE

#define	CBW_SIZE					31
#define	CBW_SIGNATURE				0x43425355
#define	CBW_IN						(1 << 7)
#define	CBW_OUT						0

#define	CSW_SIZE					13
#define	CSW_SIGNATURE				0x53425355

#define	SCSI_TEST_UNIT_READY		0x00
#define	SCSI_REQUEST_SENSE			0x03
#define	SCSI_INQUIRY				0x12
#define SCSI_START_STOP				0x1B
#define	SCSI_READ_CAPACITY			0x25
#define	SCSI_READ_10				0x28
#define	SCSI_WRITE_10				0x2A

#define	SCSI_SENSE_REPLY_SIZE		18
#define	SCSI_SENSE_NOT_READY		0x02
#define	SCSI_SENSE_MEDIUM_ERROR		0x03
#define	SCSI_SENSE_HARDWARE_ERROR	0x04

#define	USB_CLASS_MASS_STORAGE		0x08
#define	MASS_STORAGE_RBC_COMMANDS		0x01
#define	MASS_STORAGE_ATA_COMMANDS		0x02
#define	MASS_STORAGE_QIC_COMMANDS		0x03
#define	MASS_STORAGE_UFI_COMMANDS		0x04
#define	MASS_STORAGE_SFF8070_COMMANDS	0x05
#define	MASS_STORAGE_SCSI_COMMANDS		0x06
#define	MASS_STORAGE_BULK_ONLY		0x50

#define USBSTORAGE_GET_MAX_LUN		0xFE
#define USBSTORAGE_RESET			0xFF

#define	USB_ENDPOINT_BULK			0x02

#define USBSTORAGE_CYCLE_RETRIES	3

#define INVALID_LUN					-2

#define MAX_TRANSFER_SIZE_V5		(16*1024)

#define DEVLIST_MAXSIZE				8

static bool __inited = false;

/*
The following is for implementing a DISC_INTERFACE
as used by libfat
*/

static usbstorage_handle __usbfd;
static u8 __lun = 0;
static bool __mounted = false;
static u16 __vid = 0;
static u16 __pid = 0;
static bool usb2_mode=true;

static s32 __usbstorage_reset(usbstorage_handle *dev);
static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun);
s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun);

static u8 *cbw_buffer=NULL;

static s32 __send_cbw(usbstorage_handle *dev, u8 lun, u32 len, u8 flags, const u8 *cb, u8 cbLen)
{
	s32 retval = USBSTORAGE_OK;

	if(cbLen == 0 || cbLen > 16)
		return IPC_EINVAL;

	write32(((u32)cbw_buffer),bswap32(CBW_SIGNATURE));
	write32(((u32)cbw_buffer)+4,bswap32(++dev->tag));
	write32(((u32)cbw_buffer)+8,bswap32(len));
	cbw_buffer[12] = flags;
	cbw_buffer[13] = lun;
	cbw_buffer[14] = (cbLen > 6 ? 10 : 6);

	memcpy(cbw_buffer + 15, cb, cbLen);

	if(dev->suspended == 1)
	{
		USB_ResumeDevice(dev->usb_fd);
		dev->suspended = 0;
	}

	retval = USB_WriteBlkMsg(dev->usb_fd, dev->ep_out, CBW_SIZE, (void *)cbw_buffer);

	if(retval == CBW_SIZE) return USBSTORAGE_OK;
	else if(retval > 0) return USBSTORAGE_ESHORTWRITE;

	return retval;
}

static s32 __read_csw(usbstorage_handle *dev, u8 *status, u32 *dataResidue)
{
	s32 retval = USBSTORAGE_OK;
	u32 signature, tag, _dataResidue, _status;

	retval = USB_WriteBlkMsg(dev->usb_fd, dev->ep_in, CSW_SIZE, cbw_buffer);
	if(retval > 0 && retval != CSW_SIZE) return USBSTORAGE_ESHORTREAD;
	else if(retval < 0) return retval;

	signature = bswap32(read32(((u32)cbw_buffer)));
	tag = bswap32(read32(((u32)cbw_buffer)+4));
	_dataResidue = bswap32(read32(((u32)cbw_buffer)+8));
	_status = cbw_buffer[12];

	if(signature != CSW_SIGNATURE) return USBSTORAGE_ESIGNATURE;

	if(dataResidue != NULL)
		*dataResidue = _dataResidue;
	if(status != NULL)
		*status = _status;

	if(tag != dev->tag) return USBSTORAGE_ETAG;

	return USBSTORAGE_OK;
}

static s32 __cycle(usbstorage_handle *dev, u8 lun, u8 *buffer, u32 len, u8 *cb, u8 cbLen, u8 write, u8 *_status, u32 *_dataResidue)
{
	s32 retval = USBSTORAGE_OK;

	u8 status=0;
	u32 dataResidue = 0;
	u32 max_size = MAX_TRANSFER_SIZE_V5;
	u8 ep = write ? dev->ep_out : dev->ep_in;
	s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;

	do
	{
		u8 *_buffer = buffer;
		u32 _len = len;
		retries--;

		if(retval == USBSTORAGE_ETIMEDOUT)
			break;

		retval = __send_cbw(dev, lun, len, (write ? CBW_OUT:CBW_IN), cb, cbLen);

		while(_len > 0 && retval >= 0)
		{
			u32 thisLen = _len > max_size ? max_size : _len;

			if ((u32)_buffer&0x1F || !((u32)_buffer&0x10000000)) {
				if (write) memcpy(dev->buffer, _buffer, thisLen);
				retval = USB_WriteBlkMsg(dev->usb_fd, ep, thisLen, dev->buffer);
				if (!write && retval > 0)
					memcpy(_buffer, dev->buffer, retval);
			} else
				retval = USB_WriteBlkMsg(dev->usb_fd, ep, thisLen, _buffer);

			if (retval == thisLen) {
				_len -= retval;
				_buffer += retval;
			}
			else if (retval != USBSTORAGE_ETIMEDOUT)
				retval = USBSTORAGE_EDATARESIDUE;
		}

		if (retval >= 0)
			retval = __read_csw(dev, &status, &dataResidue);

		if (retval < 0) {
			if (__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
				retval = USBSTORAGE_ETIMEDOUT;
		}
	} while (retval < 0 && retries > 0);

	if(_status != NULL)
		*_status = status;
	if(_dataResidue != NULL)
		*_dataResidue = dataResidue;

	return retval;
}

static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[6];
	u8 sense[SCSI_SENSE_REPLY_SIZE];
	u8 status = 0;

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_TEST_UNIT_READY;

	retval = __cycle(dev, lun, NULL, 0, cmd, 1, 0, &status, NULL);
	if (retval < 0) return retval;

	if (status)
	{
		cmd[0] = SCSI_REQUEST_SENSE;
		cmd[1] = lun << 5;
		cmd[4] = SCSI_SENSE_REPLY_SIZE;
		memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
		retval = __cycle(dev, lun, sense, SCSI_SENSE_REPLY_SIZE, cmd, 6, 0, NULL, NULL);
		if (retval>=0) {
			switch (sense[2]&0xF) {
				case SCSI_SENSE_NOT_READY:
					return USBSTORAGE_EINIT;
				case SCSI_SENSE_MEDIUM_ERROR:
				case SCSI_SENSE_HARDWARE_ERROR:
					return USBSTORAGE_ESENSE;
			}
		}
	}

	return retval;
}

static s32 __usbstorage_reset(usbstorage_handle *dev)
{
	s32 retval = USB_WriteCtrlMsg(dev->usb_fd, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_RESET, 0, dev->interface, 0, NULL);

	udelay(60*1000);
	USB_ClearHalt(dev->usb_fd, dev->ep_in);udelay(10000); //from http://www.usb.org/developers/devclass_docs/usbmassbulk_10.pdf
	USB_ClearHalt(dev->usb_fd, dev->ep_out);udelay(10000);
	return retval;
}


s32 USBStorage_Close(usbstorage_handle *dev)
{
	__mounted = false;
	__lun = 0;
	__vid = 0;
	__pid = 0;

	if (dev->usb_fd != -1)
		USB_CloseDevice(&dev->usb_fd);

	if(dev->sector_size)
		free(dev->sector_size);

	if (dev->buffer)
		free(dev->buffer);

	memset(dev, 0, sizeof(*dev));
	dev->usb_fd = -1;
	return 0;
}

s32 USBStorage_Reset(usbstorage_handle *dev)
{
	s32 retval;

	retval = __usbstorage_reset(dev);

	return retval;
}

s32 USBStorage_Open(usbstorage_handle *dev, s32 device_id, u16 vid, u16 pid)
{
	s32 retval = -1;
	u8 conf = -1;
	u8 *max_lun;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	max_lun = malloca(1,32);
	if (!max_lun)
		return IPC_ENOMEM;

	memset(dev, 0, sizeof(*dev));
	dev->usb_fd = -1;

	dev->tag = TAG_START;

	retval = USB_OpenDevice(device_id, vid, pid, &dev->usb_fd);
	if (retval < 0)
		goto free_and_return;

	retval = USB_GetDescriptors(dev->usb_fd, &udd);
	if (retval < 0)
		goto free_and_return;

	for (iConf = 0; iConf < udd.bNumConfigurations; iConf++) {
		ucd = &udd.configurations[iConf];
		for (iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++) {
			uid = &ucd->interfaces[iInterface];
			if(uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE && /*
				   (uid->bInterfaceSubClass == MASS_STORAGE_SCSI_COMMANDS
					|| uid->bInterfaceSubClass == MASS_STORAGE_RBC_COMMANDS
					|| uid->bInterfaceSubClass == MASS_STORAGE_ATA_COMMANDS
					|| uid->bInterfaceSubClass == MASS_STORAGE_QIC_COMMANDS
					|| uid->bInterfaceSubClass == MASS_STORAGE_UFI_COMMANDS
					|| uid->bInterfaceSubClass == MASS_STORAGE_SFF8070_COMMANDS) &&*/
				   uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY)
			{
				
				if (uid->bNumEndpoints < 2)
					continue;

				dev->ep_in = dev->ep_out = 0;
				for (iEp = 0; iEp < uid->bNumEndpoints; iEp++) {
					ued = &uid->endpoints[iEp];
					if (ued->bmAttributes != USB_ENDPOINT_BULK)
						continue;

					if (ued->bEndpointAddress & USB_ENDPOINT_IN) {
						dev->ep_in = ued->bEndpointAddress;
					}
					else {
						dev->ep_out = ued->bEndpointAddress;
						if(ued->wMaxPacketSize > 64 && (dev->usb_fd>=0x20 || dev->usb_fd<-1))
							usb2_mode=true;
						else 
							usb2_mode=false;
					}
				}

				if (dev->ep_in != 0 && dev->ep_out != 0) {
					dev->configuration = ucd->bConfigurationValue;
					dev->interface = uid->bInterfaceNumber;
					dev->altInterface = uid->bAlternateSetting;
					goto found;
				}
			}
		}
	}

	USB_FreeDescriptors(&udd);
	retval = USBSTORAGE_ENOINTERFACE;
	goto free_and_return;

found:
	dev->bInterfaceSubClass = uid->bInterfaceSubClass;

	USB_FreeDescriptors(&udd);

	retval = USBSTORAGE_EINIT;
	// some devices return an error, ignore it
	USB_GetConfiguration(dev->usb_fd, &conf);

	if (conf != dev->configuration) USB_SetConfiguration(dev->usb_fd, dev->configuration);
	if (dev->altInterface !=0) USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface);

	if(!usb2_mode)
		retval = USBStorage_Reset(dev);

	dev->suspended = 0;
	retval = USB_WriteCtrlMsg(dev->usb_fd, (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_GET_MAX_LUN, 0, dev->interface, 1, max_lun);

	if (retval < 0)
		dev->max_lun = 1;
	else
		dev->max_lun = *max_lun + 1;

	if (retval == USBSTORAGE_ETIMEDOUT)
		goto free_and_return;

	retval = USBSTORAGE_OK;
	dev->sector_size = (u32 *) malloca(dev->max_lun * sizeof(u32), 32);
	memset32(dev->sector_size, 0, dev->max_lun * sizeof(u32));
	if(!dev->sector_size) {
		retval = IPC_ENOMEM;
		goto free_and_return;
	}

	/* taken from linux usbstorage module (drivers/usb/storage/transport.c)
	 *
	 * Some devices (i.e. Iomega Zip100) need this -- apparently
	 * the bulk pipes get STALLed when the GetMaxLUN request is
	 * processed.   This is, in theory, harmless to all other devices
	 * (regardless of if they stall or not).
	 *
	 * 8/9/10: If anyone wants to actually use a Zip100, they can add this back.
	 * But for now, it seems to be breaking things more than it is helping.
	 */
	//USB_ClearHalt(dev->usb_fd, dev->ep_in);
	//USB_ClearHalt(dev->usb_fd, dev->ep_out);

	if(!dev->buffer)
		dev->buffer = malloca(MAX_TRANSFER_SIZE_V5,32);

	if(!dev->buffer) {
		retval = IPC_ENOMEM;
	} else {
		//USB_DeviceRemovalNotifyAsync(dev->usb_fd,__usb_deviceremoved_cb,dev);
		retval = USBSTORAGE_OK;
	}

free_and_return:
	if (max_lun)
		free(max_lun);

	if (retval < 0) {
		USBStorage_Close(dev);
		return retval;
	}

	return 0;
}

static bool firstusbread = true;
extern u32 s_size, s_cnt;
s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors)
{
	s32 retval;
	u8 cmd[10] = {SCSI_READ_CAPACITY, lun<<5};
	u8 response[8];

	retval = __cycle(dev, lun, response, sizeof(response), cmd, sizeof(cmd), 0, NULL, NULL);
	if(retval >= 0)
	{
		if(n_sectors != NULL)
			memcpy(n_sectors, response, 4);
		if(sector_size != NULL)
			memcpy(sector_size, response + 4, 4);
		if(firstusbread)
		{
			firstusbread = false;
			s_cnt = *n_sectors;
			s_size = *sector_size;
		}
		retval = USBSTORAGE_OK;
	}

	return retval;
}

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev)
{
	return dev->max_lun;
}

s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u32 n_sectors;

	if(lun >= dev->max_lun)
		return IPC_EINVAL;

	udelay(50);
	retval = __usbstorage_clearerrors(dev, lun);
	if (retval<0)
	{
		USBStorage_Reset(dev);
		retval = __usbstorage_clearerrors(dev, lun);
	}
	
	retval = USBStorage_Inquiry(dev,  lun);
	
	retval = USBStorage_ReadCapacity(dev, lun, &dev->sector_size[lun], &n_sectors);
	if(retval >= 0 && (dev->sector_size[lun]<512 || n_sectors==0))
		return INVALID_LUN;

	return retval;
}

s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun)
{
	int n;
	s32 retval;
	u8 cmd[] = {SCSI_INQUIRY, lun << 5,0,0,36,0};
	u8 response[36];
	
	for(n=0;n<2;n++)
	{
		memset(response,0,36);
	
		retval = __cycle(dev, lun, response, 36, cmd, 6, 0, NULL, NULL);
		if(retval>=0) break;
	}

	if(retval>=0) retval=*response & 31;
	/*
	if(retval>=0)
	{
		switch(retval)
		{
			// info from http://en.wikipedia.org/wiki/SCSI_Peripheral_Device_Type
			case 5: // CDROM
			case 7: // optical memory device (e.g., some optical disks)
				__dvd_mounted = 1;
				break;
			default:
				__dvd_mounted = 0;
				break;
		}
	}
	*/
	return retval;
}

s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_READ_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >>  8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
	};

	if(lun >= dev->max_lun || dev->sector_size[lun] == 0)
		return IPC_EINVAL;

	retval = __cycle(dev, lun, buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 0, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;

	return retval;
}

s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_WRITE_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >> 8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
	};

	if(lun >= dev->max_lun || dev->sector_size[lun] == 0)
		return IPC_EINVAL;

	retval = __cycle(dev, lun, (u8 *)buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 1, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;

	return retval;
}

s32 USBStorage_Suspend(usbstorage_handle *dev)
{
	if(dev->suspended == 1)
		return USBSTORAGE_OK;

	USB_SuspendDevice(dev->usb_fd);
	dev->suspended = 1;

	return USBSTORAGE_OK;
}

bool USBStorage_Startup(void)
{
	if(__inited)
		return true;

	if(USB_Initialize() < 0)
		return false;

	if(cbw_buffer == NULL)
		cbw_buffer = (u8*)malloca(32,32);

	__inited = true;
	return __inited;
}

bool USBStorage_IsInserted(void)
{
	usb_device_entry *buffer;
	u8 device_count;
	u8 i, j;
	u16 vid, pid;
	s32 maxLun;
	s32 retval;
	u32 sectorsize, numSectors;

	if(__mounted)
	{
		// device is not a USB DVD drive - always return true
		if (__usbfd.sector_size[__lun] != 2048)
			return true;

		// check if DVD is inserted
		if (USBStorage_ReadCapacity(&__usbfd, __lun, &sectorsize, &numSectors) < 0)
			return false;
		else
			return true;
	}

	if(!__inited)
		return false;

	buffer = (usb_device_entry*)malloca(DEVLIST_MAXSIZE * sizeof(usb_device_entry),32);
	if (!buffer)
		return false;

	memset(buffer, 0, DEVLIST_MAXSIZE * sizeof(usb_device_entry));

	if (USB_GetDeviceList(buffer, DEVLIST_MAXSIZE, USB_CLASS_MASS_STORAGE, &device_count) < 0)
	{
		if (__vid != 0 || __pid != 0)
			USBStorage_Close(&__usbfd);

		free(buffer);
		return false;
	}

	udelay(100);

	if (__vid != 0 || __pid != 0) {
		for(i = 0; i < device_count; i++) {
			vid = buffer[i].vid;
			pid = buffer[i].pid;
			if(vid != 0 || pid != 0) {
				if((vid == __vid) && (pid == __pid)) {
					__mounted = true;
					free(buffer);
					udelay(50); // I don't know why I have to wait but it's needed
					return true;
				}
			}
		}
		USBStorage_Close(&__usbfd); // device changed or unplugged, return false the first time to notify to the client that he must unmount devices
		free(buffer);
		return false;
	}
	for (i = 0; i < device_count; i++) {
		vid = buffer[i].vid;
		pid = buffer[i].pid;
		if (vid == 0 || pid == 0)
			continue;
			
		if (vid == 0x0b95 && pid == 0x7720) // USB LAN
			continue;

		if (USBStorage_Open(&__usbfd, buffer[i].device_id, vid, pid) < 0)
			continue;

		maxLun = USBStorage_GetMaxLUN(&__usbfd);
		for (j = 0; j < maxLun; j++) {
			retval = USBStorage_MountLUN(&__usbfd, j);

			if (retval == INVALID_LUN)
				continue;

			if (retval < 0)
			{
				__usbstorage_reset(&__usbfd);
				continue;
			}

			__mounted = true;
			__lun = j;
			__vid = vid;
			__pid = pid;
			udelay(10000);
			break;
		}

		if (__mounted)
			break;

		USBStorage_Close(&__usbfd);
	}
	free(buffer);

	return __mounted;
}

bool USBStorage_ReadSectors(u32 sector, u32 numSectors, void *buffer)
{
	s32 retval;

	if (!__mounted)
		return false;

	retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);

	return retval >= 0;
}

bool USBStorage_WriteSectors(u32 sector, u32 numSectors, const void *buffer)
{
	s32 retval;

	if (!__mounted)
		return false;

	retval = USBStorage_Write(&__usbfd, __lun, sector, numSectors, buffer);

	return retval >= 0;
}

void USBStorage_Shutdown(void)
{
	if(__inited == false)
		return;

	if (__vid != 0 || __pid != 0)
		USBStorage_Close(&__usbfd);
	USB_Deinitialize();

	__inited = false;
}
