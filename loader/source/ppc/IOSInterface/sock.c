
#include "sock.h"
#include "cache.h"
#include "debug.h"

u32 printRetAddr(u32 retVal, u32 addr)
{
	*SO_CMD_0 = addr;
	*SO_IOCTL = 0xFF;
	while(*SO_IOCTL) ;
	return retVal;
}

// Timebase frequency is core frequency / 8.  Ignore roundoff, this
// doesn't have to be very accurate.
#define TICKS_PER_USEC (729/8)

static u32 mftb(void)
{
	u32 x;

	asm volatile("mftb %0" : "=r"(x));

	return x;
}

static void __delay(u32 ticks)
{
	u32 start = mftb();

	while (mftb() - start < ticks)
		;
}

void udelay(u32 us)
{
	__delay(TICKS_PER_USEC * us);
}

int SOStartup(void)
{
	*SO_IOCTL = 0x1F;
	while(*SO_IOCTL) ;
	int ret = (int)*SO_RETVAL;
	//debug_uint(ret);
	//debug_string("\n");
	return ret;
}

void SOGetHostId(int *id_buf)
{
	volatile u32 ip = 0;
	*SO_IOCTL = 0x10;
	while(*SO_IOCTL) ;
	ip = *SO_RETVAL;
	//debug_uint(ip);
	//debug_string("\n");
	*(volatile u32*)id_buf = ip;
}

u32 SOGetInterfaceOpt(u32 cmd, u32 len)
{
	*SO_CMD_0 = cmd;
	*SO_CMD_1 = len;
	*SO_IOCTL = 0x1C;
	while(*SO_IOCTL) ;
	//int ret = (int)*SO_RETVAL;
	//debug_uint(ret);
	//debug_string("\n");
	return (u32)SO_CMD_0;
}

void NCDGetMACAddr(char *buf)
{
	*SO_IOCTL = SO_NCD_CMD | 0x08;
	while(*SO_IOCTL) ;
	*(volatile u32*)buf = *SO_CMD_0;
	*(volatile unsigned short*)(buf+4) = (volatile unsigned short)((*SO_CMD_1)&0xFFFF);
}
