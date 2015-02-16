#ifndef __USB_OGC_H__
#define __USB_OGC_H__

#if defined(HW_RVL)

#include <gcutil.h>
#include <gctypes.h>

s32 USB_OGC_Initialize();
s32 USB_OGC_Deinitialize();

s32 USB_OGC_OpenDevice(s32 device_id,u16 vid,u16 pid,s32 *fd);
s32 USB_OGC_CloseDevice(s32 *fd);
s32 USB_OGC_CloseDeviceAsync(s32 *fd,usbcallback cb,void *usrdata);

s32 USB_OGC_GetDescriptors(s32 fd, usb_devdesc *udd);
void USB_OGC_FreeDescriptors(usb_devdesc *udd);

s32 USB_OGC_GetGenericDescriptor(s32 fd,u8 type,u8 index,u8 interface,void *data,u32 size);
s32 USB_OGC_GetHIDDescriptor(s32 fd,u8 interface,usb_hiddesc *uhd,u32 size);

s32 USB_OGC_GetDeviceDescription(s32 fd,usb_devdesc *devdesc);
s32 USB_OGC_DeviceRemovalNotifyAsync(s32 fd,usbcallback cb,void *userdata);
s32 USB_OGC_DeviceChangeNotifyAsync(u8 interface_class,usbcallback cb,void *userdata);

s32 USB_OGC_SuspendDevice(s32 fd);
s32 USB_OGC_ResumeDevice(s32 fd);

s32 USB_OGC_ReadIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData);
s32 USB_OGC_ReadIsoMsgAsync(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData,usbcallback cb,void *userdata);

s32 USB_OGC_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_OGC_ReadIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_OGC_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_OGC_ReadBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_OGC_ReadCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
s32 USB_OGC_ReadCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_OGC_WriteIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData);
s32 USB_OGC_WriteIsoMsgAsync(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData,usbcallback cb,void *userdata);

s32 USB_OGC_WriteIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_OGC_WriteIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_OGC_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_OGC_WriteBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_OGC_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
s32 USB_OGC_WriteCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_OGC_GetConfiguration(s32 fd, u8 *configuration);
s32 USB_OGC_SetConfiguration(s32 fd, u8 configuration);
s32 USB_OGC_SetAlternativeInterface(s32 fd, u8 interface, u8 alternateSetting);
s32 USB_OGC_ClearHalt(s32 fd, u8 endpointAddress);
s32 USB_OGC_GetDeviceList(usb_device_entry *descr_buffer,u8 num_descr,u8 interface_class,u8 *cnt_descr);

s32 USB_OGC_GetAsciiString(s32 fd,u8 bIndex,u16 wLangID,u16 wLength,void *rpData);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* defined(HW_RVL) */

#endif
