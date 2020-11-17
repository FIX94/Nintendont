#ifndef __IPC_H__
#define __IPC_H__	1

#define IPC_HEAP			 -1

#define IPC_OPEN_NONE		  0
#define IPC_OPEN_READ		  1
#define IPC_OPEN_WRITE		  2
#define IPC_OPEN_RW			  (IPC_OPEN_READ|IPC_OPEN_WRITE)

#define IPC_MAXPATH_LEN		 64

#define IPC_OK				  0
#define IPC_EINVAL			 -4
#define IPC_ENOHEAP			 -5
#define IPC_ENOENT			 -6
#define IPC_EQUEUEFULL		 -8
#define IPC_ENOMEM			-22

struct ioctl_vector 
{
	void *data;
	unsigned int len;
} __attribute__((packed));

/* IOCTL vector */
typedef struct iovec 
{
	void *data;
	u32   len;
} ioctlv;

struct ipcmessage
{
	unsigned int command;			// 0
	unsigned int result;			// 4
	union
	{
		unsigned int fd;			// 8
	};
	union 
	{ 
		struct
	 	{
			char *device;			// 12
			unsigned int mode;		// 16
			unsigned int resultfd;	// 20
		} open;
	
		struct 
		{
			void *data;
			unsigned int length;
		} read, write;
		
		struct 
	 	{
			int offset;
			int origin;
		} seek;
		
		struct 
	 	{
			unsigned int command;

			unsigned int *buffer_in;
			unsigned int length_in;
			unsigned int *buffer_io;
			unsigned int length_io;
		} ioctl;

		struct
		{
			unsigned int command;		// C

			unsigned int argc_in;		// 10
			unsigned int argc_io;		// 14
			struct ioctl_vector *argv;	// 18
		} ioctlv;
	};
} __attribute__((packed)); 

#define IOS_OPEN				0x01
#define IOS_CLOSE				0x02
#define IOS_READ				0x03
#define IOS_WRITE				0x04
#define IOS_SEEK				0x05
#define IOS_IOCTL				0x06
#define IOS_IOCTLV				0x07
#define IOS_ASYNC				0x08

#endif
