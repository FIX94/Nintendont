
#ifndef _SOCK_H_
#define _SOCK_H_

#define		SO_BASE		0xD3026900

#define		SO_IOCTL	(volatile u32*)(SO_BASE+0x00)
#define		SO_RETVAL	(volatile u32*)(SO_BASE+0x04)
#define		SO_CMD_0	(volatile u32*)(SO_BASE+0x08)
#define		SO_CMD_1	(volatile u32*)(SO_BASE+0x0C)
#define		SO_CMD_2	(volatile u32*)(SO_BASE+0x10)
#define		SO_CMD_3	(volatile u32*)(SO_BASE+0x14)

#define		SO_NCD_CMD	0x1000

#endif
