
#ifndef _SOCK_H_
#define _SOCK_H_

void SOCKInit();
void SOCKShutdown();
void SOCKUpdateRegisters();
void SOCKInterrupt(void);
bool SOCKCheckTimer(void);

#define		SO_BASE		0x13026900

#define		SO_IOCTL	(SO_BASE+0x00)
#define		SO_RETVAL	(SO_BASE+0x04)
#define		SO_CMD_0	(SO_BASE+0x08)
#define		SO_CMD_1	(SO_BASE+0x0C)
#define		SO_CMD_2	(SO_BASE+0x10)
#define		SO_CMD_3	(SO_BASE+0x14)
#define		SO_CMD_4	(SO_BASE+0x18)

#define		SO_NCD_CMD	0x1000

#endif
