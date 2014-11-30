#ifndef __USBSTORAGE_H__
#define __USBSTORAGE_H__

#define	USBSTORAGE_OK			0
#define	USBSTORAGE_ENOINTERFACE		-10000
#define	USBSTORAGE_ESENSE		-10001
#define	USBSTORAGE_ESHORTWRITE		-10002
#define	USBSTORAGE_ESHORTREAD		-10003
#define	USBSTORAGE_ESIGNATURE		-10004
#define	USBSTORAGE_ETAG			-10005
#define	USBSTORAGE_ESTATUS		-10006
#define	USBSTORAGE_EDATARESIDUE		-10007
#define	USBSTORAGE_ETIMEDOUT		-10008
#define	USBSTORAGE_EINIT		-10009
#define USBSTORAGE_PROCESSING	-10010

typedef struct
{
	u8 configuration;
	u32 interface;
	u32 altInterface;
	u8 bInterfaceSubClass;

	u8 ep_in;
	u8 ep_out;

	u8 max_lun;
	u32 *sector_size;

	s32 usb_fd;

	//mutex_t lock;
	//syswd_t alarm;
	s32 retval;

	u32 tag;
	u8 suspended;

	u8 *buffer;
} usbstorage_handle;

#define B_RAW_DEVICE_DATA_IN 0x01
#define B_RAW_DEVICE_COMMAND 0

typedef struct {
   uint8_t         command[16];
   uint8_t         command_length;
   uint8_t         flags;
   uint8_t         scsi_status;
   void*           data;
   size_t          data_length;
} raw_device_command;

bool USBStorage_Startup(void);
bool USBStorage_IsInserted(void);
bool USBStorage_ReadSectors(u32 sector, u32 numSectors, void *buffer);
bool USBStorage_WriteSectors(u32 sector, u32 numSectors, const void *buffer);
void USBStorage_Shutdown(void);

#endif /* __USBSTORAGE_H__ */
