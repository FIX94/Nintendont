#ifndef __USBSTORAGE_OGC_H__
#define __USBSTORAGE_OGC_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <ogc/mutex.h>
#include <ogc/disc_io.h>
#include <ogc/system.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

s32 USBStorageOGC_Initialize();

s32 USBStorageOGC_Open(usbstorage_handle *dev, s32 device_id, u16 vid, u16 pid);
s32 USBStorageOGC_Close(usbstorage_handle *dev);
s32 USBStorageOGC_Reset(usbstorage_handle *dev);

s32 USBStorageOGC_GetMaxLUN(usbstorage_handle *dev);
s32 USBStorageOGC_MountLUN(usbstorage_handle *dev, u8 lun);
s32 USBStorageOGC_Suspend(usbstorage_handle *dev);
s32 USBStorageOGC_IsDVD();
s32 USBStorageOGC_ioctl(int request, ...);

s32 USBStorageOGC_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors);
s32 USBStorageOGC_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer);
s32 USBStorageOGC_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer);
s32 USBStorageOGC_StartStop(usbstorage_handle *dev, u8 lun, u8 lo_ej, u8 start, u8 imm);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* HW_RVL */

#endif /* __USBSTORAGE_H__ */
