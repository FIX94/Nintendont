/*-------------------------------------------------------------

usbstorage.c -- Bulk-only USB mass storage support

Copyright (C) 2008
Sven Peter (svpe) <svpe@gmx.net>

quick port to ehci/ios: Kwiirk

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

#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

//#define	HEAP_SIZE					(32*1024)
#define	TAG_START					0x1//BADC0DE 0x22112211

#define	CBW_SIZE					31
#define	CBW_SIGNATURE				0x43425355
#define	CBW_IN						(1 << 7)
#define	CBW_OUT						0

#define	CSW_SIZE					13
#define	CSW_SIGNATURE				0x53425355

#define	SCSI_TEST_UNIT_READY		0x00
#define	SCSI_INQUIRY				0x12
#define	SCSI_REQUEST_SENSE			0x03
#define	SCSI_START_STOP				0x1b
#define	SCSI_READ_CAPACITY			0x25
#define	SCSI_READ_10				0x28
#define	SCSI_WRITE_10				0x2A

#define	SCSI_SENSE_REPLY_SIZE		18
#define	SCSI_SENSE_NOT_READY		0x02
#define	SCSI_SENSE_MEDIUM_ERROR		0x03
#define	SCSI_SENSE_HARDWARE_ERROR	0x04

#define	USB_CLASS_MASS_STORAGE		0x08
#define USB_CLASS_HUB				0x09

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

#define MAX_TRANSFER_SIZE			4096

#define DEVLIST_MAXSIZE    			8

#include "debug.h"

static int ums_init_done = 0;
static int used_port = 0;

extern int handshake_mode; // 0->timeout force -ENODEV 1->timeout return -ETIMEDOUT

static bool first_access = true;

static usbstorage_handle __usbfd;
static u8 __lun = 16;
static u8 __mounted = 0;
static u16 __vid = 0;
static u16 __pid = 0;

static usb_devdescr _old_udd; // used for device change protection

s32 try_status = 0;

static s32 __usbstorage_reset(usbstorage_handle *dev);
static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun);
static s32 __usbstorage_start_stop(usbstorage_handle *dev, u8 lun, u8 start_stop);

extern u32 usb_timeout;

// ehci driver has its own timeout.
static s32 __USB_BlkMsgTimeout(usbstorage_handle *dev, u8 bEndpoint, u16 wLength, void *rpData)
{
	return USB_WriteBlkMsg(dev->usb_fd, bEndpoint, wLength, rpData);
}

static s32 __USB_CtrlMsgTimeout(usbstorage_handle *dev, u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, u16 wLength, void *rpData)
{
	return USB_WriteCtrlMsg(dev->usb_fd, bmRequestType, bmRequest, wValue, wIndex, wLength, rpData);
}

static s32 __send_cbw(usbstorage_handle *dev, u8 lun, u32 len, u8 flags, const u8 *cb, u8 cbLen)
{
	s32 retval = USBSTORAGE_OK;

	if(cbLen == 0 || cbLen > 16)
		return -EINVAL;
		
	memset(dev->buffer, 0, CBW_SIZE);

	((u32*)dev->buffer)[0]=cpu_to_le32(CBW_SIGNATURE);
	((u32*)dev->buffer)[1]=cpu_to_le32(dev->tag);
	((u32*)dev->buffer)[2]=cpu_to_le32(len);
	dev->buffer[12] = flags;
	dev->buffer[13] = lun;
	if(dev->ata_protocol)
		dev->buffer[14] = 12; 
	else
		dev->buffer[14] = (cbLen > 6 ? 0x10 : 6);

	memcpy(dev->buffer + 15, (void *)cb, cbLen);

	retval = __USB_BlkMsgTimeout(dev, dev->ep_out, CBW_SIZE, (void *)dev->buffer);

	if(retval == CBW_SIZE) 
		return USBSTORAGE_OK;
	else if(retval >= 0) 
		return USBSTORAGE_ESHORTWRITE;

	return retval;
}

static s32 __read_csw(usbstorage_handle *dev, u8 *status, u32 *dataResidue)
{
	s32 retval = USBSTORAGE_OK;
	u32 signature, tag, _dataResidue, _status;

	memset(dev->buffer, 0xff, CSW_SIZE);

	retval = __USB_BlkMsgTimeout(dev, dev->ep_in, CSW_SIZE, dev->buffer);

	if(retval >= 0 && retval != CSW_SIZE) 
		return USBSTORAGE_ESHORTREAD;
	else if(retval < 0) 
		return retval;

	signature = le32_to_cpu(((u32*)dev->buffer)[0]);
	tag = le32_to_cpu(((u32*)dev->buffer)[1]);
	_dataResidue = le32_to_cpu(((u32*)dev->buffer)[2]);
	_status = dev->buffer[12];

	if(signature != CSW_SIGNATURE) 
		return USBSTORAGE_ESIGNATURE;

	if(dataResidue != NULL)
		*dataResidue = _dataResidue;
	if(status != NULL)
		*status = _status;

	if(tag != dev->tag) return USBSTORAGE_ETAG;
	dev->tag++;

	return USBSTORAGE_OK;
}

static s32 __cycle(usbstorage_handle *dev, u8 lun, u8 *buffer, u32 len, u8 *cb, u8 cbLen, u8 write, u8 *_status, u32 *_dataResidue)
{
	s32 retval = USBSTORAGE_OK;

	u8 status = 0;
	u32 dataResidue = 0;
	u32 thisLen;
	
	u8 *buffer2 = buffer;
	u32 len2=len;

	s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;

	do
	{
		if(retval == -ENODEV) 
		{
			unplug_device = 1;
			return -ENODEV;
		}
		
		if(retval < 0)
          retval = __usbstorage_reset(dev);
		  
		  
		retries--;
		if(retval == USBSTORAGE_ETIMEDOUT)
			break;
			
		if(retval < 0) 
			continue;
			
		buffer = buffer2;
		len = len2;

		if(write)
		{
			retval = __send_cbw(dev, lun, len, CBW_OUT, cb, cbLen);
			
			if(retval < 0)
				continue;
			
			while(len > 0)
			{
				thisLen = len;
				retval = __USB_BlkMsgTimeout(dev, dev->ep_out, thisLen, buffer);

				if(retval==-ENODEV || retval==-ETIMEDOUT) 
					break;

				if(retval < 0)
					continue;

				if(retval != thisLen && len > 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					continue;
				}
				len -= retval;
				buffer += retval;
			}

			if(retval < 0)
				continue;
		}
		else
		{
			retval = __send_cbw(dev, lun, len, CBW_IN, cb, cbLen);

			if(retval < 0)
				continue;
			
			while(len > 0)
			{
				thisLen = len;
				retval = __USB_BlkMsgTimeout(dev, dev->ep_in, thisLen, buffer);
				
				if(retval==-ENODEV || retval==-ETIMEDOUT) 
					break;
				
				if(retval < 0)
					continue;

				len -= retval;
				buffer += retval;

				if(retval != thisLen)
				{
					retval = -1;
					continue;
				}
			}

			if(retval < 0)					
				continue;
		}

		retval = __read_csw(dev, &status, &dataResidue);

		if(retval < 0)
			continue;

		retval = USBSTORAGE_OK;
	} while(retval < 0 && retries > 0);

	if(retval < 0 && retval != USBSTORAGE_ETIMEDOUT)
	{
		if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
			retval = USBSTORAGE_ETIMEDOUT;
	}
	
	if(retval < 0 && retries <= 0 && handshake_mode==0) 
	{
		unplug_device = 1;
		return -ENODEV;
	}

	if(retval < 0 && retval != USBSTORAGE_ETIMEDOUT)
	{
		if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
			retval = USBSTORAGE_ETIMEDOUT;
	}

	if(_status != NULL)
		*_status = status;
	if(_dataResidue != NULL)
		*_dataResidue = dataResidue;

	return retval;
}

static s32 __usbstorage_start_stop(usbstorage_handle *dev, u8 lun, u8 start_stop)
{
	s32 retval = 0;
#if 0	
	u8 cmd[16];	
	u8 status = 0;
	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_START_STOP;
	cmd[1] = (lun << 5) | 1;
	cmd[4] = start_stop & 3;
	cmd[5] = 0;
	retval = __cycle(dev, lun, NULL, 0, cmd, 6, 0, &status, NULL);
#endif
	return retval;	
}

static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[16];
	u8 *sense= USB_Alloc(SCSI_SENSE_REPLY_SIZE);
	u8 status = 0;
	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_TEST_UNIT_READY;
	
	int n;
	
	if(!sense) 
		return -ENOMEM;
		
	for(n = 0; n < 5; n++)
	{
		retval = __cycle(dev, lun, NULL, 0, cmd, 1, 1, &status, NULL);
		
		if(retval == -ENODEV) 
			goto error;
		
		if(retval == 0) 
			break;
	}

	if(status != 0)
	{
		cmd[0] = SCSI_REQUEST_SENSE;
		cmd[1] = lun << 5;
		cmd[4] = SCSI_SENSE_REPLY_SIZE;
		cmd[5] = 0;
		memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
		retval = __cycle(dev, lun, sense, SCSI_SENSE_REPLY_SIZE, cmd, 6, 0, NULL, NULL);
		
		if(retval < 0) 
			goto error;

		status = sense[2] & 0x0F;
		if(status == SCSI_SENSE_NOT_READY || status == SCSI_SENSE_MEDIUM_ERROR || status == SCSI_SENSE_HARDWARE_ERROR) 
			retval = USBSTORAGE_ESENSE;
	}
error:
	USB_Free(sense);
	return retval;
}

static s32 __usbstorage_reset(usbstorage_handle *dev)
{
	s32 retval;
	u32 old_usb_timeout = usb_timeout; 
	usb_timeout = 1000*1000;	
	int retry = 0;
	
retry:
	if (retry >= 1)
	{
		u8 conf;
		retval = ehci_reset_device(dev->usb_fd);
		msleep(10);
		if(retval==-ENODEV) 
			return retval;

		if(retval < 0 && retval != -7004)
			goto end;
                // reset configuration
		if(USB_GetConfiguration(dev->usb_fd, &conf) < 0)
			goto end;
		if(/*conf != dev->configuration &&*/ USB_SetConfiguration(dev->usb_fd, dev->configuration) < 0)
			goto end;
		if(dev->altInterface != 0 && USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface) < 0)
			goto end;
		if(__lun != 16)
		{
			if(USBStorage_MountLUN(&__usbfd, __lun) < 0)
				goto end;
		}
	}
	
	retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_RESET, 0, dev->interface, 0, NULL);
	if(retval < 0 && retval != -7004)
		goto end;
		
	msleep(10);
	
	retval = USB_ClearHalt(dev->usb_fd, dev->ep_in);
	
	if(retval < 0)
		goto end;
			
	msleep(10);
	retval = USB_ClearHalt(dev->usb_fd, dev->ep_out);

	msleep(50);
    usb_timeout=old_usb_timeout; 
	return retval;

end:
	if(retval==-ENODEV)
		return retval;

	if(retry < 5)
	{
		msleep(100);
		retry++;
		goto retry;
	}
	usb_timeout = old_usb_timeout;
	return retval;
}

s32 USBStorage_Open(usbstorage_handle *dev, struct ehci_device *fd)
{
	s32 retval = -1;
	u8 conf, *max_lun = NULL;
	u32 iConf, iInterface, iEp;
	static usb_devdescr udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;
	
	__lun = 16;

	max_lun = USB_Alloc(1);
	if(max_lun==NULL) 	
		return -ENOMEM;

	memset(dev, 0, sizeof(*dev));

	dev->tag = TAG_START;
	dev->usb_fd = fd;

	retval = USB_GetDescriptors(dev->usb_fd, &udd);
	if(retval < 0)
		goto free_and_return;
		
	if(ums_init_done)
	{
		if(memcmp((void *)&_old_udd, (void *)&udd, sizeof(usb_devdescr)-4)) 
		{
			USB_Free(max_lun);
			USB_FreeDescriptors(&udd);
			return -ENODEV;
		}
	}
	
	_old_udd= udd;
	try_status=-128;

	for(iConf = 0; iConf < udd.bNumConfigurations; iConf++)
	{
		ucd = &udd.configurations[iConf];		
		for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
		{
			uid = &ucd->interfaces[iInterface];
			if(uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE &&
			   (uid->bInterfaceSubClass == MASS_STORAGE_SCSI_COMMANDS
				|| uid->bInterfaceSubClass == MASS_STORAGE_RBC_COMMANDS
                || uid->bInterfaceSubClass == MASS_STORAGE_ATA_COMMANDS
                || uid->bInterfaceSubClass == MASS_STORAGE_QIC_COMMANDS
				|| uid->bInterfaceSubClass == MASS_STORAGE_UFI_COMMANDS
				|| uid->bInterfaceSubClass == MASS_STORAGE_SFF8070_COMMANDS) &&
			   uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY && uid->bNumEndpoints>=2)
			{
				dev->ata_protocol = 0;
                if(uid->bInterfaceSubClass != MASS_STORAGE_SCSI_COMMANDS || uid->bInterfaceSubClass != MASS_STORAGE_RBC_COMMANDS)
					dev->ata_protocol = 1;

				dev->ep_in = dev->ep_out = 0;
				for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
				{
					ued = &uid->endpoints[iEp];
					if(ued->bmAttributes != USB_ENDPOINT_BULK)
						continue;

					if(ued->bEndpointAddress & USB_ENDPOINT_IN)
						dev->ep_in = ued->bEndpointAddress;
					else
						dev->ep_out = ued->bEndpointAddress;
				}
				if(dev->ep_in != 0 && dev->ep_out != 0)
				{
					dev->configuration = ucd->bConfigurationValue;
					dev->interface = uid->bInterfaceNumber;
					dev->altInterface = uid->bAlternateSetting;
					goto found;
				}
			}
			else
			{
				if(uid->endpoints != NULL)
					USB_Free(uid->endpoints);uid->endpoints= NULL;
				if(uid->extra != NULL)
					USB_Free(uid->extra);uid->extra=NULL;

				if(uid->bInterfaceClass == USB_CLASS_HUB)
				{
					retval = USBSTORAGE_ENOINTERFACE;
					try_status= -20000;

					USB_FreeDescriptors(&udd);
        
					goto free_and_return;
				}

				if(uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE && uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY && uid->bNumEndpoints>=2)
					try_status= -(10000 + uid->bInterfaceSubClass);
			}
		}
	}

	USB_FreeDescriptors(&udd);
	retval = USBSTORAGE_ENOINTERFACE;
	goto free_and_return;

found:
	USB_FreeDescriptors(&udd);

	retval = USBSTORAGE_EINIT;
	try_status=-1201;
	if(USB_GetConfiguration(dev->usb_fd, &conf) < 0)
		goto free_and_return;
		
	try_status=-1202;	
	if(conf != dev->configuration && USB_SetConfiguration(dev->usb_fd, dev->configuration) < 0)
		goto free_and_return;
	
	try_status=-1203;	
	if(dev->altInterface != 0 && USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface) < 0)
		goto free_and_return;
		
	try_status=-1204;

	retval = USBStorage_Reset(dev);
	if(retval < 0)
		goto free_and_return;

	dev->max_lun = 8;
	retval = USBSTORAGE_OK;
	dev->buffer = USB_Alloc(MAX_TRANSFER_SIZE+16);
	
	if(dev->buffer == NULL) 
	{
		try_status = -1205;
		retval = -ENOMEM;
	}
	else 
		retval = USBSTORAGE_OK;

free_and_return:
	if(max_lun != NULL) 
		USB_Free(max_lun);
	if(retval < 0)
	{
		if(dev->buffer != NULL)
			USB_Free(dev->buffer);
			
		memset(dev, 0, sizeof(*dev));
		return retval;
	}
	return 0;
}

s32 USBStorage_Close(usbstorage_handle *dev)
{
	if(dev->buffer != NULL)
		USB_Free(dev->buffer);
		
	memset(dev, 0, sizeof(*dev));
	return 0;
}

s32 USBStorage_Reset(usbstorage_handle *dev)
{
	s32 retval;
	retval = __usbstorage_reset(dev);

	return retval;
}

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev)
{
	return dev->max_lun;
}

s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	int f = handshake_mode;

	if(lun >= dev->max_lun)
		return -EINVAL;
		
	usb_timeout = 1000 * 1000;
	handshake_mode = 1;

	retval= __usbstorage_start_stop(dev, lun, 1);
	if(retval < 0)
		goto ret;
		
	usb_timeout = 1000 * 1000;
	
	retval = __usbstorage_clearerrors(dev, lun);
	if(retval < 0)
		goto ret;

	retval = USBStorage_Inquiry(dev, lun);
	if(retval < 0)
		goto ret;

	retval = USBStorage_ReadCapacity(dev, lun, &dev->sector_size[lun], &dev->n_sector[lun]);
ret:
	handshake_mode = f;
	return retval;
}

s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[] = {SCSI_INQUIRY, lun << 5,0,0,36,0};
	u8 *response = USB_Alloc(36);

	retval = __cycle(dev, lun, response, 36, cmd, 6, 0, NULL, NULL);
	USB_Free(response);
	return retval;
}

s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors)
{
	s32 retval;
	u8 cmd[] = {SCSI_READ_CAPACITY, lun << 5, 0, 0, 0, 0, 0, 0, 0, 0};
	u8 *response = USB_Alloc(8);
	u32 val;
	
	if(!response) 
		return -ENOMEM;
	
	retval = __cycle(dev, lun, response, 8, cmd, sizeof(cmd), 0, NULL, NULL);
	if(retval >= 0)
	{				
		memcpy(&val, response, 4);
		if(n_sectors != NULL)
			*n_sectors = be32_to_cpu(val);
		
		memcpy(&val, response + 4, 4);
		if(sector_size != NULL)
			*sector_size = be32_to_cpu(val);
			
		retval = USBSTORAGE_OK;
	}
	USB_Free(response);
	return retval;
}

s32 __USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
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
		return -EINVAL;
	
	retval = __cycle(dev, lun, buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 0, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
		
	return retval;
}

s32 __USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
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
		return -EINVAL;
	retval = __cycle(dev, lun, (u8 *)buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 1, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
		
	return retval;
}

s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
	u32 max_sectors = n_sectors;
	u32 sectors;
	s32 ret = -1;

	if(n_sectors * dev->sector_size[lun] > 32768) 
		max_sectors = 32768 / dev->sector_size[lun];

	while(n_sectors > 0)
	{
		sectors = n_sectors>max_sectors ? max_sectors : n_sectors;
		ret = __USBStorage_Read(dev, lun, sector, sectors, buffer);
		if(ret<0) return ret;
		
		n_sectors-=sectors;
		sector += sectors;
		buffer += sectors * dev->sector_size[lun];
	}
	return ret;	
}

s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
{
	u32 max_sectors = n_sectors;
	u32 sectors;
	s32 ret = -1;

	if((n_sectors * dev->sector_size[lun]) > 32768) 
		max_sectors= 32768 / dev->sector_size[lun];

	while(n_sectors > 0)
	{
		sectors = n_sectors > max_sectors ? max_sectors : n_sectors;
		ret = __USBStorage_Write(dev, lun, sector, sectors, buffer);
		if(ret < 0) 
			return ret;
		
		n_sectors -= sectors;
		sector += sectors;
		buffer += sectors * dev->sector_size[lun];
	}
	return ret;	
}

/*s32 USBStorage_Read_Stress(u32 sector, u32 numSectors, void *buffer)
{
   s32 retval;
   int i;
   if(__mounted != 1)
	   return false;
   
   for(i = 0; i < 512; i++)
   {
		retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);
		sector += numSectors;
		if(retval == USBSTORAGE_ETIMEDOUT)
		{
			__mounted = 0;
			USBStorage_Close(&__usbfd);
		}
		if(retval < 0)
			return false;
   }
   return true;

}*/

// temp function before libfat is available */
s32 USBStorage_Try_Device(struct ehci_device *fd)
{
	int maxLun, j, retval;
	int test_max_lun = 1;
	
	try_status = -120;
	
	if(USBStorage_Open(&__usbfd, fd) < 0)
		return -EINVAL;
	
	maxLun = 1;
	__usbfd.max_lun = 1;
		
	
	j = 0;
	while(1)
	{
		retval = USBStorage_MountLUN(&__usbfd, j);
		if(retval == USBSTORAGE_ETIMEDOUT)
		{
			try_status=-121;
			__mounted = 0;
			USBStorage_Close(&__usbfd); 
			return -EINVAL;
		}		

		if(retval < 0)
		{
			if(test_max_lun)
			{
				__usbfd.max_lun = 0;
				retval = __USB_CtrlMsgTimeout(&__usbfd, (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_GET_MAX_LUN, 0, __usbfd.interface, 1, &__usbfd.max_lun);

				if(retval < 0)
					__usbfd.max_lun = 1;
				else 
					__usbfd.max_lun++;

				maxLun = __usbfd.max_lun;
				test_max_lun = 0;
			}
			else 
				j++;
			
			if(j>=maxLun) 
				break;

			continue;
		}		
		
		__vid = fd->desc.idVendor;
		__pid = fd->desc.idProduct;
		__mounted = 1;
		__lun = j;
		usb_timeout = 1000 * 1000;
		try_status = 0;
		return 0;
	}
	try_status = -122;
	__mounted = 0;
	USBStorage_Close(&__usbfd);
	
	return -EINVAL;
}

/*void USBStorage_Umount(void)
{
	if(!ums_init_done) 
		return;
	
	if(__mounted && !unplug_device)
	{
		if(__usbstorage_start_stop(&__usbfd, __lun, 0x0) == 0) // stop
			msleep(1000);
	}

	USBStorage_Close(&__usbfd);__lun = 16;
	__mounted=0;
	ums_init_done = 0;
	unplug_device = 0;
}*/

s32 USBStorage_Init(void)
{
	int i;
	
	//dbgprintf("usbstorage init %d\n", ums_init_done);
	if(ums_init_done)
		return 0;
		
	try_status =- 1;
		
	for(i = 0; i < ehci->num_port; i++)
	{
		struct ehci_device *dev = &ehci->devices[i];
		
		dev->port = i;
		
		if(dev->id != 0)
		{		
			handshake_mode = 1;		
			if(ehci_reset_port(0) >= 0)
			{		
		
				if(USBStorage_Try_Device(dev) == 0)
				{
					first_access = true;
					handshake_mode = 0;
					ums_init_done = 1;
					unplug_device = 0;
					return 0;
				}
			}
		}
		else
		{
			u32 status;
			handshake_mode = 1;
			status = ehci_readl(&ehci->regs->port_status[0]);

			if(status & 1)
			{
				if(ehci_reset_port2(0) < 0)
				{
					msleep(100);
					ehci_reset_port(0);
				}
				return -101;
			}
			else 
			{
				return -100;
			}
		}
	}
	
	return try_status;
}

s32 USBStorage_Get_Capacity(u32*sector_size)
{
	if(__mounted == 1)
	{
		if(sector_size)
			*sector_size = __usbfd.sector_size[__lun];

		return __usbfd.n_sector[__lun];
	}
   
	return 0;
}

int unplug_procedure(void)
{
	int retval = 1;

	if(unplug_device != 0)
	{			
		if(__usbfd.usb_fd)
		{
			if(ehci_reset_port2(/*__usbfd.usb_fd->port*/0) >= 0)	
			{
				if(__usbfd.buffer != NULL)
					USB_Free(__usbfd.buffer);
					
				__usbfd.buffer = NULL;

				if(ehci_reset_port(0) >= 0)
				{
					handshake_mode=1;
					if(USBStorage_Try_Device(__usbfd.usb_fd)==0) 
					{
						retval = 0;
						unplug_device = 0;
					}
					handshake_mode=0;
				}

			}
		}
		msleep(100);
	}

	return retval;
}

s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer)
{
	s32 retval=0;
	int retry;
	if(__mounted != 1)
		return false;
   
	for(retry = 0; retry < 16; retry++)
	{
		if(retry > 12) 
			retry = 12; // infinite loop

		if(!unplug_procedure())	
			retval = 0;
	
		if(retval == USBSTORAGE_ETIMEDOUT && retry > 0)
			unplug_device = 1;

		if(unplug_device != 0) 
			continue;

		usb_timeout = 1000 * 1000; // 4 seconds to wait
		if(retval >= 0)
			retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);
				
		usb_timeout = 1000 * 1000;
		if(unplug_device != 0) 
			continue;

		if(retval >= 0) 
			break;
	}

   if(retval < 0)
       return false;
	   
   return true;
}

s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer)
{
	s32 retval=0;
	int retry;

	if(__mounted != 1)
		return false;

	for(retry = 0; retry < 16; retry++)
	{
		if(retry > 12) 
			retry = 12; // infinite loop	

		if(!unplug_procedure())
			retval=0;

		if(retval == USBSTORAGE_ETIMEDOUT && retry > 0)
			unplug_device=1;

		if(unplug_device!=0 ) 
			continue;

		usb_timeout= 1000 * 1000; // 4 seconds to wait
		if(retval >=0)
			retval = USBStorage_Write(&__usbfd, __lun, sector, numSectors, buffer);
			
		usb_timeout= 1000 * 1000;
		if(unplug_device!=0 ) 
			continue;
			
		if(retval >= 0) 
			break;
	}

   if(retval < 0)
       return false;
	   
   return true;
}