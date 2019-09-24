/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2019  FIX94

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "sock.h"
#include "cache.h"

//#define DEBUG_SOCK 1

static so_struct_arm *sock_entry_arm = (so_struct_arm*)SO_BASE_ARM;
static so_struct_buf *sock_data_buf = (so_struct_buf*)SO_DATA_BUF;

extern u32 IsInited;
static u32 socket_started = 0;
static u32 interface_started = 0;
//for optimization purposes
static int conf_cache = -1, ip_cache = 0, link_cache = 0;
static u8 used[SO_TOTAL_FDS];
static u64 *queue = (u64*)SO_BASE_PPC;

//general location of interrupt stuff
static volatile u32 *osIrqHandlerAddr = (u32*)0x80003040;
static volatile u32 *sysIrqMask = (u32*)0x800000C4;
static volatile u32 *piIrqMask = (u32*)0xCC003004;
//from our asm
extern int disableIRQs(void);
extern int restoreIRQs(int val);
extern void setSOStartStatus(int status);
//defined in linker script
extern void (*OSSleepThread)(void *queue);
extern void (*OSWakeupThread)(void *queue);

#ifndef DEBUG_SOCK
#define OSReport(...)
#else
//mario kart ntsc-u
//static void (*OSReport)(char *in, ...) = (void*)0x800E8078;
//kirby air ride ntsc-u
//static void (*OSReport)(char *in, ...) = (void*)0x803D4CE8;
//1080 avalanche ntsc-u
//static void (*OSReport)(char *in, ...) = (void*)0x80136174;
//pso plus ntsc-u
//static void (*OSReport)(char *in, ...) = (void*)0x80371DF8;
//pso 1.0 ntsc-u
static void (*OSReport)(char *in, ...) = (void*)0x8036E6B4;
#endif

int getFreeFD()
{
	int val = disableIRQs();
	int i;
	for(i = 0; i < SO_TOTAL_FDS; i++)
	{
		if(used[i] == 0)
		{
			used[i] = 1;
			break;
		}
	}
	restoreIRQs(val);
	return i;
}

void freeUpFD(int i)
{
	int val = disableIRQs();
	used[i] = 0;
	restoreIRQs(val);
}

void SOInit(void)
{
	sync_before_read((void*)SO_HSP_LOC, 0x20);
	if(IsInited) //done already
		return;
	if(*SO_HSP_LOC == 0) //game patching failed
		return;
	int val = disableIRQs();
	//clear out thread queue
	int i;
	for(i = 0; i < SO_TOTAL_FDS; i++)
	{
		queue[i] = 0;
		used[i] = 0;
	}
	//set custom interrupt handler
	osIrqHandlerAddr[0x1A] = *SO_HSP_LOC;
	//enable custom interrupt
	*sysIrqMask &= ~0x20;
	*piIrqMask |= 0x2000;
	//ready to accept requests
	IsInited = 1;
	sync_after_write((void*)SO_HSP_LOC, 0x20);
	OSReport("SOInit done\n");
	restoreIRQs(val);
}

int doCall(int call, int fd, int doIRQ)
{
	//OSReport("doCall start %i %i\n", call, fd);
	sock_entry_arm[fd].ioctl = call;
	sync_after_write(&sock_entry_arm[fd], 0x20);
	if(doIRQ)
		OSSleepThread((void*)(&queue[fd]));
	else
	{
		while(sock_entry_arm[fd].ioctl)
		{
			u32 garbage = 0xFFFF;
			while(garbage--) ;
			sync_before_read(&sock_entry_arm[fd], 0x20);
		}
	}
	sync_before_read(&sock_entry_arm[fd], 0x20);
	//OSReport("doCall done %i\n", sock_entry_arm[fd].retval);
	return sock_entry_arm[fd].retval;
}

static inline void reset_cache_vals()
{
	//reset these
	conf_cache = -1;
	ip_cache = 0;
	link_cache = 0;
}

int doStartSOInterface(int fd, int doIRQ)
{
	if(!IsInited) //no interface to call open
		return 0;
	if(!interface_started)
	{
		if(doCall(so_startinterface, fd, doIRQ) >= 0)
		{
			int val = disableIRQs();
			interface_started = 1;
			reset_cache_vals();
			restoreIRQs(val);
			//started interface successfully
			return 1;
		}
		//call failed...
		return 0;
	}
	//already started
	return 1;
}

int doSOGetInterfaceOpt(int cmd, void *buf, int len, int fd, int doIRQ)
{
	if(doStartSOInterface(fd, doIRQ))
	{
		sock_entry_arm[fd].cmd0 = cmd;
		sock_entry_arm[fd].cmd1 = (u32)buf;
		sock_entry_arm[fd].cmd2 = len;
		if(doCall(so_getinterfaceopt, fd, doIRQ) >= 0)
		{
			sync_before_read(buf, len);
			return 1;
		}
	}
	return 0;
}

int doSOSetInterfaceOpt(int cmd, char *buf, int len, int fd, int doIRQ)
{
	if(doStartSOInterface(fd, doIRQ))
	{
		sock_entry_arm[fd].cmd0 = cmd;
		sock_entry_arm[fd].cmd1 = (u32)buf;
		sock_entry_arm[fd].cmd2 = len;
		sync_after_write(buf, len);
		if(doCall(so_setinterfaceopt, fd, doIRQ) >= 0)
			return 1;
	}
	return 0;
}

//gets in some config, but just use nwc call
//with default config instead to start up SO
int SOStartup(void)
{
	if(!IsInited) //no interface to call open
		return 0;
	if(socket_started) //already started
		return 0;
	int ret;
	int fd = getFreeFD();
	do {
		ret = doCall(nwc_startup, fd, 1);
	} while(ret < 0);
	freeUpFD(fd);
	int val = disableIRQs();
	socket_started = 1;
	//enable game SO status too
	setSOStartStatus(1);
	//also enables interface
	interface_started = 1;
	reset_cache_vals();
	restoreIRQs(val);
	OSReport("SOStartup done\n");
	return 0;
}

int SOCleanup(void)
{
	if(!IsInited) //no interface to call open
		return 0;
	if(!socket_started) //already cleaned up
		return 0;
	int ret;
	int fd = getFreeFD();
	do {
		ret = doCall(nwc_cleanup, fd, 1);
	} while(ret < 0);
	freeUpFD(fd);
	int val = disableIRQs();
	socket_started = 0;
	//disable game SO status too
	setSOStartStatus(0);
	//also disables interface
	interface_started = 0;
	reset_cache_vals();
	restoreIRQs(val);
	OSReport("SOCleanup done\n");
	return 0;
}

int SOSocket(int a, int b, int c)
{
	if(!socket_started)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = a;
	sock_entry_arm[fd].cmd1 = b;
	sock_entry_arm[fd].cmd2 = c;
	ret = doCall(so_socket, fd, 1);
	freeUpFD(fd);
	OSReport("SOSocket done %08x\n", ret);
	return ret;
}

int SOClose(int snum)
{
	if(!socket_started)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	ret = doCall(so_close, fd, 1);
	freeUpFD(fd);
	OSReport("SOClose done %08x\n", ret);
	return ret;
}

int SOListen(int snum, int list)
{
	if(!socket_started)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = list;
	ret = doCall(so_listen, fd, 1);
	freeUpFD(fd);
	OSReport("SOListen done %08x\n", ret);
	return ret;
}

int SOAccept(int snum, char *sock)
{
	//quit if no socket running or socket input size not 8
	if(!socket_started || (sock && sock[0] != 8))
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	if(!sock) //no return request
	{
		sock_entry_arm[fd].cmd1 = 0;
		sock_entry_arm[fd].cmd2 = 0;
		ret = doCall(so_accept, fd, 1);
	}
	else //requests actual return
	{
		sock_entry_arm[fd].cmd1 = *(volatile u32*)(sock+0);
		sock_entry_arm[fd].cmd2 = *(volatile u32*)(sock+4);
		ret = doCall(so_accept, fd, 1);
		if(ret >= 0) //copy back return value
		{
			*(volatile u32*)(sock+0) = sock_entry_arm[fd].cmd1;
			*(volatile u32*)(sock+4) = sock_entry_arm[fd].cmd2;
		}
	}
	freeUpFD(fd);
	OSReport("SOAccept done %08x\n", ret);
	return ret;
}

int SOBind(int snum, char *sock)
{
	//quit if no socket running or no input or input size not 8
	if(!socket_started || !sock || sock[0] != 8)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = *(volatile u32*)(sock+0x0);
	sock_entry_arm[fd].cmd2 = *(volatile u32*)(sock+0x4);
	ret = doCall(so_bind, fd, 1);
	freeUpFD(fd);
	OSReport("SOBind done %08x\n", ret);
	return ret;
}

int SOConnect(int snum, char *sock)
{
	//quit if no socket running or no input or input size not 8
	if(!socket_started || !sock || sock[0] != 8)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = *(volatile u32*)(sock+0x0);
	sock_entry_arm[fd].cmd2 = *(volatile u32*)(sock+0x4);
	ret = doCall(so_connect, fd, 1);
	freeUpFD(fd);
	OSReport("SOConnect done %08x\n", ret);
	return ret;
}

int SOShutdown(int snum, int msg)
{
	if(!socket_started)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = msg;
	ret = doCall(so_shutdown, fd, 1);
	freeUpFD(fd);
	OSReport("SOShutdown done %08x\n", ret);
	return ret;
}

int SORecvFrom(int snum, char *buf, int len, int flags, char *sock)
{
	OSReport("SORecvFrom %i %08x %i %i %08x\n", snum, buf, len, flags, sock);
	if(!socket_started || !buf || len < 0 || (sock && sock[0] != 8))
		return -1;
	int ret;
	int fd = getFreeFD();
	//max len of 32k
	if(len >= 0x8000)
		len = 0x8000;
	//set common values
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = (u32)sock_data_buf[fd].buf;
	sock_entry_arm[fd].cmd2 = len;
	sock_entry_arm[fd].cmd3 = flags;
	if(!sock)
	{
		sock_entry_arm[fd].cmd4 = 0;
		sock_entry_arm[fd].cmd5 = 0;
		ret = doCall(so_recvfrom, fd, 1);
	}
	else
	{
		sock_entry_arm[fd].cmd4 = *(volatile u32*)(sock+0);
		sock_entry_arm[fd].cmd5 = *(volatile u32*)(sock+4);
		ret = doCall(so_recvfrom, fd, 1);
		if(ret >= 0) //copy back sock
		{
			*(volatile u32*)(sock+0) = sock_entry_arm[fd].cmd4;
			*(volatile u32*)(sock+4) = sock_entry_arm[fd].cmd5;
		}
	}
	//copy back actually received data
	if(ret > 0)
	{
		sync_before_read((void*)sock_data_buf[fd].buf, ret);
		int i;
		for(i = 0; i < ret; i++)
			*(volatile u8*)(buf+i) = *(volatile u8*)(sock_data_buf[fd].buf+i);
	}
	freeUpFD(fd);
	OSReport("SORecvFrom done %08x\n", ret);
	return ret;
}

int SOSendTo(int snum, char *buf, int len, int flags, char *sock)
{
	if(!socket_started || !buf || len < 0 || (sock && sock[0] != 8))
		return -1;
	int ret;
	int fd = getFreeFD();
	//max len of 32k
	if(len >= 0x8000)
		len = 0x8000;
	//copy over data to send
	if(len > 0)
	{
		int i;
		for(i = 0; i < len; i++)
			*(volatile u8*)(sock_data_buf[fd].buf+i) = *(volatile u8*)(buf+i);
		sync_after_write((void*)sock_data_buf[fd].buf, len);
	}
	//set common values
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = (u32)sock_data_buf[fd].buf;
	sock_entry_arm[fd].cmd2 = len;
	sock_entry_arm[fd].cmd3 = flags;
	if(!sock)
	{
		sock_entry_arm[fd].cmd4 = 0;
		sock_entry_arm[fd].cmd5 = 0;
	}
	else
	{
		sock_entry_arm[fd].cmd4 = *(volatile u32*)(sock+0);
		sock_entry_arm[fd].cmd5 = *(volatile u32*)(sock+4);
	}
	ret = doCall(so_sendto, fd, 1);
	freeUpFD(fd);
	OSReport("SOSendTo done %08x\n", ret);
	return ret;
}

int SOSetSockOpt(int snum, int ctype, int cmd, char *buf, int len)
{
	if(!socket_started || len > 0x14 || len < 0)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = (u32)sock_data_buf[fd].buf;
	sock_entry_arm[fd].cmd1 = 0x24; //fixed out size
	*(volatile u32*)(sock_data_buf[fd].buf+0x0) = snum;
	*(volatile u32*)(sock_data_buf[fd].buf+0x4) = ctype;
	*(volatile u32*)(sock_data_buf[fd].buf+0x8) = cmd;
	*(volatile u32*)(sock_data_buf[fd].buf+0xC) = len;
	if(len)
	{
		int i;
		if(buf)
		{
			for(i = 0; i < len; i++)
				*(volatile u8*)(sock_data_buf[fd].buf+0x10+i) = *(volatile u8*)(buf+i);
		}
		else
		{
			for(i = 0; i < len; i++)
				*(volatile u8*)(sock_data_buf[fd].buf+0x10+i) = 0;
		}
	}
	sync_after_write((void*)sock_data_buf[fd].buf, 0x24);
	ret = doCall(so_setsockopt, fd, 1);
	freeUpFD(fd);
	OSReport("SOSetSockOpt done %08x\n", ret);
	return ret;
}

//is actually ... as last arg, but was only ever assigned
//one variable arg anyways, so this functions the same as the original
int SOFcntl(int snum, int cmd, int arg)
{
	if(!socket_started)
		return -1;
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = cmd;
	sock_entry_arm[fd].cmd2 = arg;
	ret = doCall(so_fcntl, fd, 1);
	OSReport("SOFcntl done %08x\n", ret);
	freeUpFD(fd);
	return ret;
}

int SOPoll(oldpoll_params *params, int num, unsigned long long time)
{
	if(!socket_started || !params)
		return -1;
	int ret;
	int fd = getFreeFD();
	newpoll_params *newparams = (newpoll_params*)sock_data_buf[fd].buf;
	int i;
	for(i = 0; i < num; i++)
	{
		newparams[i].socket = params[i].socket;
		newparams[i].flagsIn = params[i].flagsIn;
		newparams[i].flagsOut = params[i].flagsOut;
	}
	sync_after_write(newparams, sizeof(newpoll_params)*num);
	sock_entry_arm[fd].cmd0 = (u32)newparams;
	sock_entry_arm[fd].cmd1 = sizeof(newpoll_params)*num;
	sock_entry_arm[fd].cmd2 = (u32)(time>>32);
	sock_entry_arm[fd].cmd3 = (u32)(time&0xFFFFFFFF);
	ret = doCall(so_poll, fd, 1);
	if(ret >= 0)
	{
		sync_before_read(newparams, sizeof(newpoll_params)*num);
		for(i = 0; i < num; i++)
		{
			params[i].socket = newparams[i].socket;
			params[i].flagsIn = newparams[i].flagsIn;
			params[i].flagsOut = newparams[i].flagsOut;
		}
	}
	OSReport("SOPoll done %08x\n", ret);
	freeUpFD(fd);
	return ret;
}

void IPGetMacAddr(char *interface_in, char *retval)
{
	if(!retval)
		return;
	if(interface_in)
	{
		*(volatile u32*)(retval+0) = *(volatile u32*)(interface_in+0x38);
		*(volatile u16*)(retval+4) = *(volatile u16*)(interface_in+0x3C);
		return;
	}
	//default interface request
	int fd = getFreeFD();
	//do getmacaddr, note specifically for mario kart dd:
	//using an irq to wait for the result will crash for some reason,
	//possibly due to the specific game booting thread its executed from
	if(doSOGetInterfaceOpt(0x1004, (void*)sock_data_buf[fd].buf, 6, fd, 0)) //read out from IOS address
	{
		*(volatile u32*)(retval+0) = *(volatile u32*)(sock_data_buf[fd].buf+0);
		*(volatile u16*)(retval+4) = *(volatile u16*)(sock_data_buf[fd].buf+4);
	}
	else //just return 0
	{
		*(volatile u32*)(retval+0) = 0;
		*(volatile u16*)(retval+4) = 0;
	}
	freeUpFD(fd);
	OSReport("IPGetMacAddr done %08x%04x\n", *(u32*)retval, *(unsigned short*)(retval+4));
}

void IPGetBroadcastAddr(char *interface_in, u32 *retval)
{
	if(!retval)
		return;
	if(interface_in)
	{
		*retval = *(volatile u32*)(interface_in+0x48);
		return;
	}
	//default interface request
	int fd = getFreeFD();
	if(doSOGetInterfaceOpt(0x4003, (void*)sock_data_buf[fd].buf, 12, fd, 1)) //read out from IOS address
		*retval = *(volatile u32*)(sock_data_buf[fd].buf+8);
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
	OSReport("IPGetBroadcastAddr done %08x\n", *retval);
}

void IPGetNetmask(char *interface_in, u32 *retval)
{
	if(!retval)
		return;
	if(interface_in)
	{
		*retval = *(volatile u32*)(interface_in+0x48);
		return;
	}
	//default interface request
	int fd = getFreeFD();
	if(doSOGetInterfaceOpt(0x4003, (void*)sock_data_buf[fd].buf, 12, fd, 1)) //read out from IOS address
		*retval = *(volatile u32*)(sock_data_buf[fd].buf+4);
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
	OSReport("IPGetNetmask done %08x\n", *retval);
}

void IPGetAddr(char *interface_in, u32 *retval)
{
	if(!retval)
		return;
	if(interface_in)
	{
		*retval = *(volatile u32*)(interface_in+0x44);
		return;
	}
	//speedup
	if(ip_cache)
	{
		*retval = ip_cache;
		return;
	}
	//default interface request
	int fd = getFreeFD();
	if(doSOGetInterfaceOpt(0x4003, (void*)sock_data_buf[fd].buf, 12, fd, 1)) //read out from IOS address
		*retval = *(volatile u32*)sock_data_buf[fd].buf;
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
	ip_cache = *retval;
	OSReport("IPGetAddr done %08x\n", *retval);
}

void IPGetMtu(char *interface_in, u32 *retval)
{
	if(!retval)
		return;
	if(interface_in)
	{
		*retval = *(volatile u32*)(interface_in+0x40);
		return;
	}
	//default interface request
	int fd = getFreeFD();
	if(doSOGetInterfaceOpt(0x4004, (void*)sock_data_buf[fd].buf, 4, fd, 1)) //read out from IOS address
		*retval = *(volatile u32*)sock_data_buf[fd].buf;
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
	OSReport("IPGetMtu done %08x\n", *retval);
}

void IPGetLinkState(char *interface_in, u32 *retval)
{
	if(!retval)
		return;
	if(interface_in)
	{
		*retval = *(volatile u32*)(interface_in+4);
		return;
	}
	//speedup
	if(link_cache)
	{
		*retval = link_cache;
		return;
	}
	//default interface request
	int fd = getFreeFD();
	//do getlinkstate
	if(doSOGetInterfaceOpt(0x1005, (void*)sock_data_buf[fd].buf, 4, fd, 1)) //read out from IOS address
		*retval = *(volatile u32*)sock_data_buf[fd].buf;
	else //just say link down
		*retval = 0;
	freeUpFD(fd);
	link_cache = *retval;
	OSReport("IPGetLinkState done %08x\n", *retval);
}

int IPGetConfigError(char *interface_in)
{
	if(interface_in)
		return *(volatile u32*)(interface_in+8);
	//speedup
	if(conf_cache == 0)
		return 0;
	int ret;
	//default interface request
	int fd = getFreeFD();
	//do getconfigerror
	if(doSOGetInterfaceOpt(0x1003, (void*)sock_data_buf[fd].buf, 4, fd, 1)) //read out from IOS address
		ret = *(volatile u32*)sock_data_buf[fd].buf;
	else //return some negative number I guess
		ret = -1;
	freeUpFD(fd);
	conf_cache = ret;
	OSReport("IPGetConfigError done %08x\n", ret);
	return ret;
}

int IPSetConfigError(char *interface_in, int err)
{
	int ret = IPGetConfigError(interface_in);
	if(interface_in)
	{ 
		if(ret == 0) //overwrite interface value and ret
		{
			ret = err;
			*(volatile u32*)(interface_in+8) = err;
		}
		return ret; //returns current interface value
	}
	//speedup
	if(ret == 0 && err == 0)
		return 0;
	//default interface request
	int fd = getFreeFD();
	if(ret == 0)
	{
		ret = err;
		*(volatile u32*)sock_data_buf[fd].buf = err;
		//do setconfigerror
		doSOSetInterfaceOpt(0x1003, (void*)sock_data_buf[fd].buf, 4, fd, 1);
	}
	freeUpFD(fd);
	conf_cache = ret;
	OSReport("IPSetConfigError done %08x\n", ret);
	return ret; //returns current interface value
}

int IPClearConfigError(char *interface_in)
{
	int ret = IPGetConfigError(interface_in);
	if(interface_in)
	{
		//clear out current interface value
		*(volatile u32*)(interface_in+8) = 0;
		return ret; //returns last interface value
	}
	//speedup
	if(ret == 0)
		return 0;
	//default interface request
	int fd = getFreeFD();
	//clear out current interface value
	*(volatile u32*)sock_data_buf[fd].buf = 0;
	//do clearconfigerror
	doSOSetInterfaceOpt(0x1003, (void*)sock_data_buf[fd].buf, 4, fd, 1);
	freeUpFD(fd);
	conf_cache = ret;
	OSReport("IPClearConfigError done %08x\n", ret);
	return ret; //returns last interface value
}


/* All the PSO related hooks follow here */

//defined in linker script
extern int (*OSCreateThread)(void *thread, void *func, u32 funcvar, void *stack_start, u32 stack_size, u32 prio, u32 params);
extern void (*OSResumeThread)(void *thread);
extern void setDHCPStateOffset(int status);
//allow 4 threads just for receives, 10 may be original but we only have so much space....
#define PSO_MAX_FDS 4
static u8 recvThread[PSO_MAX_FDS][0x800] __attribute__((aligned(32)));
static u8 recvStack[PSO_MAX_FDS][0x800] __attribute__((aligned(32)));
static volatile u32 recvArg[PSO_MAX_FDS][4];
static volatile u32 recvused[PSO_MAX_FDS];
static u64 recvQueue[PSO_MAX_FDS];

static u32 dns_arr[2];
static u32 dns_num = 0;
static char *dns_names[SO_TOTAL_FDS];
static int dns_namelen[SO_TOTAL_FDS];

int getFreeRecvFD()
{
	int val = disableIRQs();
	int i;
	for(i = 0; i < PSO_MAX_FDS; i++)
	{
		if(recvused[i] == 0)
		{
			recvused[i] = 1;
			break;
		}
	}
	restoreIRQs(val);
	return i;
}

void freeUpRecvFD(int i)
{
	int val = disableIRQs();
	recvused[i] = 0;
	restoreIRQs(val);
}

static volatile u32 recvRunning = 0;
static void recvHandle(u32 num)
{
	//initial sleep
	OSSleepThread(&recvQueue[num]);
	//OSReport("Initial wakeup %i\n", num);
	//on wakeup, check if we quit
	while(recvRunning)
	{
		int sock = recvArg[num][0];
		char *buf = (char*)recvArg[num][1];
		int len = recvArg[num][2];
		void *cb = (void*)recvArg[num][3];
		int ret = SORecvFrom(sock, buf, len, 0, 0);
		if(cb) ((void(*)(int,int))(cb))(ret, sock);
		freeUpRecvFD(num);
		OSSleepThread(&recvQueue[num]);
		//OSReport("wakeup again %i\n", num);
	}
	//OSReport("end %i\n", num);
}

static u32 avetcp_inited = 0;
int avetcp_init()
{
	OSReport("avetcp_init\n");
	SOInit();
	if(!avetcp_inited)
	{
		//start up receive threads
		recvRunning = 1;
		int i;
		for(i = 0; i < PSO_MAX_FDS; i++)
		{
			recvQueue[i] = 0;
			OSCreateThread(recvThread[i], recvHandle, i, recvStack[i] + 0x800, 0x800, 5, 0);
			OSResumeThread(recvThread[i]);
		}
		avetcp_inited = 1;
	}
	return 0;
}

int avetcp_term()
{
	OSReport("avetcp_term\n");
	if(avetcp_inited)
	{
		//wait for any remaining threads
		int i;
		int remain;
		do
		{
			remain = 0;
			u32 garbage = 0xFFFFFF;
			while(garbage--) ;
			for(i = 0; i < PSO_MAX_FDS; i++)
			{
				if(recvused[i])
				{
					remain = 1;
					break;
				}
			}
		}
		while(remain);
		//all recv threads now sleeping, end them
		recvRunning = 0;
		for(i = 0; i < PSO_MAX_FDS; i++)
		{
			OSWakeupThread(&recvQueue[i]);
			u32 garbage = 0xFFFFFF;
			while(garbage--) ;
		}
		avetcp_inited = 0;
	}
	return 0;
}

int dns_set_server(u32 *addr)
{
	u32 num = dns_num;
	OSReport("dns_set_server %i %08x\n", num, addr[1]);
	if(addr[0] == 4 && addr[1]) dns_arr[num] = addr[1];
	//write to ios if at least primary is set
	if(num == 1 && dns_arr[0])
	{
		int fd = getFreeFD();
		*(volatile u32*)(sock_data_buf[fd].buf+0) = dns_arr[0];
		*(volatile u32*)(sock_data_buf[fd].buf+4) = dns_arr[1];
		//do set dns server
		doSOSetInterfaceOpt(0xB003, (void*)sock_data_buf[fd].buf, 8, fd, 1);
		freeUpFD(fd);
	}
	dns_num++;
	return num;
}

int dns_clear_server()
{
	OSReport("dns_clear_server\n");
	int ret = dns_num;
	dns_arr[0] = 0;
	dns_arr[1] = 0;
	dns_num = 0;
	return ret;
}

int dns_open_addr(char *name, int len, u32 unk1)
{
	OSReport("dns_open_addr %s\n", name);
	int fd = getFreeFD();
	dns_names[fd] = name;
	//include 0 at end
	dns_namelen[fd] = len+1;
	return fd;
}

int dns_get_addr(int fd, u32 *arr, u32 unk1, u32 unk2, u32 unk3, u32 unk4)
{
	OSReport("dns_get_addr %i\n", fd);
	int i;
	for(i = 0; i < dns_namelen[fd]; i++)
		sock_data_buf[fd].buf[i] = dns_names[fd][i];
	sync_after_write((void*)sock_data_buf[fd].buf, dns_namelen[fd]);
	sock_entry_arm[fd].cmd0 = (u32)sock_data_buf[fd].buf;
	sock_entry_arm[fd].cmd1 = dns_namelen[fd];
	//giving it 0x200 for a name is overkill, should work fine
	sock_entry_arm[fd].cmd2 = (u32)sock_data_buf[fd].buf+0x200;
	sock_entry_arm[fd].cmd3 = 0x460;
	int ret = doCall(so_gethostbyname, fd, 1);
	if(ret >= 0)
	{
		sync_before_read((void*)sock_data_buf[fd].buf+0x200, 0x460);
		u16 addrlen = *(u16*)(sock_data_buf[fd].buf+0x20A);
		if(addrlen == 4)
		{
			//offset represents address difference between original IOS memory region and our memory region
			u32 ppc_offset = ((u32)sock_data_buf[fd].buf+0x210) - *(u32*)(sock_data_buf[fd].buf+0x200);
			//resolve ip entry from its various pointers
			u32 ip_arr_addr_ptr = (*(u32*)(sock_data_buf[fd].buf+0x20C)) + ppc_offset;
			u32 ip_arr_entry0_ptr = (*(u32*)(ip_arr_addr_ptr)) + ppc_offset;
			u32 ip_arr_entry0 = *(u32*)ip_arr_entry0_ptr;
			//write resolved ip back into game
			arr[0] = addrlen;
			arr[1] = ip_arr_entry0;
			OSReport("dns_get_addr ret %08x\n", arr[1]);
		}
		return 0;
	}
	return ret;
}

int dns_close(int fd)
{
	OSReport("dns_close %i\n", fd);
	freeUpFD(fd);
	return 0;
}

int tcp_create()
{
	OSReport("tcp_create\n");
	return SOSocket(2,1,0);
}

int tcp_setsockopt(int sock, int cmd, u8 *buf)
{
	OSReport("tcp_setsockopt %i %08x %i\n", sock, cmd, *(u32*)buf);
	return SOSetSockOpt(sock, 6, cmd, (char*)buf, 4);
}

int tcp_bind(int sock, u32 *arr, u16 port)
{
	OSReport("tcp_bind %i %08x %i\n", sock, arr[1], sock);
	u32 input[2];
	input[0] = 0x08020000 | port;
	input[1] = arr[1];
	return SOBind(sock,(char*)input);
}

int tcp_connect(int sock, u32 *arr, u16 port, u32 unk1)
{
	OSReport("tcp_connect %i %08x %i\n", sock, arr[1], sock);
	u32 input[2];
	input[0] = 0x08020000 | port;
	input[1] = arr[1];
	return SOConnect(sock,(char*)input);
}

int tcp_stat(int sock, short *someweirdthing, u32 *unk1free, u32 *sendfree, u32 *unk2free)
{
	OSReport("tcp_stat %i\n", sock);
	//this on init has to be 4 or more to work
	if(someweirdthing) *someweirdthing = 4;
	//always say we have the full 32k to send
	if(sendfree) *sendfree = 0x8000;
	return 0;
}

//assumes cb always set to 0 (sync call)
typedef struct _sendarr {
	u16 len;
	char *buf;
} sendarr;

int tcp_send(int sock, u32 cb, int pcks, sendarr *arr)
{
	OSReport("tcp_send %i\n", sock);
	int i;
	for(i = 0; i < pcks; i++)
		SOSendTo(sock, arr[i].buf, arr[i].len, 0, 0);
	return 0;
}

int tcp_receive(int sock, u32 cb, int len, char *buf)
{
	OSReport("tcp_receive %i\n", sock);
	if(cb)
	{
		int fd = getFreeRecvFD();
		recvArg[fd][0] = sock;
		recvArg[fd][1] = (u32)buf;
		recvArg[fd][2] = len;
		recvArg[fd][3] = cb;
		OSWakeupThread(&recvQueue[fd]);
	}
	else
		SORecvFrom(sock, buf, len, 0, 0);
	return 0;
}

int tcp_abort(int sock)
{
	OSReport("tcp_abort %i\n", sock);
	return SOShutdown(sock, 0);
}

int tcp_delete(int sock)
{
	OSReport("tcp_delete %i\n", sock);
	return SOClose(sock);
}

int DHCP_request_nb(u32 unk1, u32 unk2, u32 unk3, u32 unk4, u32 *ip, u32 *netmask, u32 *bcast, u32 tries)
{
	OSReport("DHCP_request_nb\n");
	SOStartup();
	*ip = 0;
	while(*ip == 0)
	{
		u32 garbage = 0xFFFFFFF;
		while(garbage--) ;
		IPGetAddr(0, ip);
	}
	IPGetNetmask(0, netmask);
	IPGetBroadcastAddr(0, bcast);
	dns_arr[0] = 0;
	dns_arr[1] = 0;
	dns_num = 0;
	setDHCPStateOffset(3); //done
	return 0;
}

int DHCP_get_dns(u32 unk, char *domain, u32 *addr1, u32 *addr2)
{
	OSReport("DHCP_get_dns\n");
	//default interface request
	int fd = getFreeFD();
	if(doSOGetInterfaceOpt(0xB003, (void*)sock_data_buf[fd].buf, 8, fd, 1)) //read out from IOS address
	{
		*addr1 = *(volatile u32*)(sock_data_buf[fd].buf+0);
		*addr2 = *(volatile u32*)(sock_data_buf[fd].buf+4);
		OSReport("DHCP_get_dns ret %08x %08x\n", *addr1, *addr2);
	}
	freeUpFD(fd);

	return 0;
}

int DHCP_release()
{
	OSReport("DHCP_release\n");
	setDHCPStateOffset(0); //off
	return SOCleanup();
}
