#ifndef __USBSTORAGE_H__
#define __USBSTORAGE_H__


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

#include "ehci.h"


#define	USBSTORAGE_OK			0
#define	USBSTORAGE_ENOINTERFACE		-10000
#define	USBSTORAGE_ESENSE		-10001
#define	USBSTORAGE_ESHORTWRITE		-10002
#define	USBSTORAGE_ESHORTREAD		-10003
#define	USBSTORAGE_ESIGNATURE		-10004
#define	USBSTORAGE_ETAG			-10005
#define	USBSTORAGE_ESTATUS		-10006
#define	USBSTORAGE_EDATARESIDUE		-10007
#define	USBSTORAGE_ETIMEDOUT		-ETIMEDOUT
#define	USBSTORAGE_EINIT		-10009

typedef struct
{
	u8 configuration;
	u32 interface;
	u32 altInterface;

	u8 ep_in;
	u8 ep_out;

	u8 max_lun;
	u32 sector_size[16];
	u32 n_sector[16];

	struct ehci_device * usb_fd;

	//mutex_t lock;
	//cond_t cond;
	s32 retval;

	u32 tag;
	u8 ata_protocol;

	u8 *buffer;
} usbstorage_handle;

s32 USBStorage_Initialize(void);

s32 USBStorage_Open(usbstorage_handle *dev, struct ehci_device *fd);
s32 USBStorage_Close(usbstorage_handle *dev);
s32 USBStorage_Reset(usbstorage_handle *dev);

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev);
s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun);
s32 USBStorage_Suspend(usbstorage_handle *dev);

s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors);
s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer);
s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer);
s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun);

#define DEVICE_TYPE_WII_USB (('W'<<24)|('U'<<16)|('S'<<8)|'B')

s32 USBStorage_Try_Device(struct ehci_device *fd);
void USBStorage_Umount(void);
#ifdef __cplusplus
   }
#endif /* __cplusplus */


#endif /* __USBSTORAGE_H__ */
