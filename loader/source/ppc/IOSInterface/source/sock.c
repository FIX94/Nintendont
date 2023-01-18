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
//#include "string.h"
//#include <stdarg.h>

//#define DEBUG_SOCK 1

//total overall allocated
#define SO_TOTAL_FDS 64
//total depending on game
extern volatile u16 SOCurrentTotalFDs;

static volatile unsigned char *ioctl_ppc = (unsigned char*)(SO_STRUCT_BUF+0x00);
static volatile unsigned char *ioctl_arm = (unsigned char*)(SO_STRUCT_BUF+0x40);
static so_struct_arm *sock_entry_arm = (so_struct_arm*)(SO_STRUCT_BUF+0x80);

extern volatile u16 SOShift;
extern volatile u32 IsInited;
static volatile u32 socket_started = 0;
static volatile u32 interface_started = 0;
//for optimization purposes
static volatile int conf_cache = -1, ip_cache = 0, link_cache = 0;

static volatile u8 so_used[SO_TOTAL_FDS];
static u64 *so_queue = (u64*)SO_BASE_PPC;
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
//static void (*OSReport)(char *in, ...) = (void*)0x8036E6B4;
//homeland 1.0
//static void (*OSReport)(char *in, ...) = (void*)0x8004D438;

#endif

int getFreeFD()
{
	int i;
	do {
		int val = disableIRQs();
		sync_before_read((void*)ioctl_arm, 0x40);
		for(i = 0; i < SOCurrentTotalFDs; i++)
		{
			if(ioctl_arm[i] == 0 && so_used[i] == 0)
			{
				so_used[i] = 1;
				break;
			}
		}
		restoreIRQs(val);
		if(i == SOCurrentTotalFDs)
		{
			u32 garbage = 0xFFFF;
			while(garbage--) ;
		}
	} while(i == SOCurrentTotalFDs);
	return i;
}

void freeUpFD(int i)
{
	int val = disableIRQs();
	so_used[i] = 0;
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
		so_queue[i] = 0;
		so_used[i] = 0;
	}
	//set custom interrupt handler
	osIrqHandlerAddr[0x1A] = *SO_HSP_LOC;
	//enable custom interrupt
	*sysIrqMask &= ~0x20;
	*piIrqMask |= 0x2000;
	//ready to accept requests
	IsInited = 1;
	sync_after_write((void*)SO_HSP_LOC, 0x20);
	
	restoreIRQs(val);
	//OSReport("SOInit done\n");
}

void doCallStart(int call, int fd, void *queue)
{
	//OSReport("doCall start %i %i %i\n", call, fd, queue);
	//set to busy
	sock_entry_arm[fd].busy = 1;
	sync_after_write(&sock_entry_arm[fd], 0x20);
	//set and sync with irqs off
	int val = disableIRQs();
	ioctl_ppc[fd] = call;
	sync_after_store((void*)ioctl_ppc, 0x40);
	if(queue)
	{
		OSSleepThread(queue);
		//only restore after it woke up,
		//so that it sleeps when its already
		//possibly done and about to be woken up
		restoreIRQs(val);
	}
	else
	{
		restoreIRQs(val);
		//just loop, safe enough...
		while(sock_entry_arm[fd].busy)
		{
			u32 garbage = 0xFFFF;
			while(garbage--) ;
			sync_before_read(&sock_entry_arm[fd], 0x20);
		}
	}
}
int doCallEnd(int fd)
{
	//set and sync with irqs off
	int val = disableIRQs();
	ioctl_ppc[fd] = 0;
	sync_after_store((void*)ioctl_ppc, 0x40);
	restoreIRQs(val);
	//get ret and return
	sync_before_read(&sock_entry_arm[fd], 0x20);
	//OSReport("doCall done %i\n", sock_entry_arm[fd].retval);
	return sock_entry_arm[fd].retval;
}

//synced version used by SO interface
int doCall(int call, int fd, void *queue)
{
	doCallStart(call, fd, queue);
	return doCallEnd(fd);
}

static inline void reset_cache_vals()
{
	//reset these
	conf_cache = -1;
	ip_cache = 0;
	link_cache = 0;
}

int doStartSOInterface(void *queue)
{
	if(!IsInited) //no interface to call open
		return 0;
	//not yet started, beging checks
	if(!interface_started)
	{
		//important: assign own FD to this sub-call, or else
		//there may not be enough time for it to free up on IOS side!
		int startfd = getFreeFD();
		if(doCall(so_startinterface, startfd, queue ? &so_queue[startfd] : 0) >= 0)
		{
			//started interface successfully
			interface_started = 1;
			reset_cache_vals();
		}
		freeUpFD(startfd);
	}
	return interface_started;
}

int doSOGetInterfaceOpt(int cmd, void *buf, int len, int fd, void *queue)
{
	if(doStartSOInterface(queue))
	{
		sock_entry_arm[fd].cmd0 = cmd;
		sock_entry_arm[fd].cmd1 = (u32)buf;
		sock_entry_arm[fd].cmd2 = len;
		if(doCall(so_getinterfaceopt, fd, queue) >= 0)
		{
			sync_before_read(buf, len);
			return 1;
		}
	}
	return 0;
}

int doSOSetInterfaceOpt(int cmd, char *buf, int len, int fd, void *queue)
{
	if(doStartSOInterface(queue))
	{
		sock_entry_arm[fd].cmd0 = cmd;
		sock_entry_arm[fd].cmd1 = (u32)buf;
		sock_entry_arm[fd].cmd2 = len;
		sync_after_write(buf, len);
		if(doCall(so_setinterfaceopt, fd, queue) >= 0)
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
		ret = doCall(nwc_startup, fd, &so_queue[fd]);
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
		ret = doCall(nwc_cleanup, fd, &so_queue[fd]);
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
	//OSReport("SOCleanup done\n");
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
	ret = doCall(so_socket, fd, &so_queue[fd]);
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
	ret = doCall(so_close, fd, &so_queue[fd]);
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
	ret = doCall(so_listen, fd, &so_queue[fd]);
	freeUpFD(fd);
	OSReport("SOListen done %08x\n", ret);
	return ret;
}

static int _SOSockCmd(int snum, char *sock, int socmd, int wantres)
{
	int ret;
	int fd = getFreeFD();
	sock_entry_arm[fd].cmd0 = snum;
	if(!sock) //no sock input
	{
		sock_entry_arm[fd].cmd1 = 0;
		sock_entry_arm[fd].cmd2 = 0;
		ret = doCall(socmd, fd, &so_queue[fd]);
	}
	else if(!wantres) //no return request
	{
		sock_entry_arm[fd].cmd1 = *(volatile u32*)(sock+0);
		sock_entry_arm[fd].cmd2 = *(volatile u32*)(sock+4);
		ret = doCall(socmd, fd, &so_queue[fd]);
	}
	else //requests actual return
	{
		sock_entry_arm[fd].cmd1 = *(volatile u32*)(sock+0);
		sock_entry_arm[fd].cmd2 = *(volatile u32*)(sock+4);
		ret = doCall(socmd, fd, &so_queue[fd]);
		if(ret >= 0) //copy back return value
		{
			*(volatile u32*)(sock+0) = sock_entry_arm[fd].cmd1;
			*(volatile u32*)(sock+4) = sock_entry_arm[fd].cmd2;
		}
	}
	freeUpFD(fd);
	return ret;
}

int SOAccept(int snum, char *sock)
{
	//quit if no socket running or socket input size not 8
	if(!socket_started || (sock && sock[0] != 8))
		return -1;
	int ret = _SOSockCmd(snum, sock, so_accept, sock ? 1 : 0);
	OSReport("SOAccept done %08x\n", ret);
	return ret;
}

int SOBind(int snum, char *sock)
{
	//quit if no socket running or no input or input size not 8
	if(!socket_started || !sock || sock[0] != 8)
		return -1;
	int ret = _SOSockCmd(snum, sock, so_bind, 0);
	OSReport("SOBind done %08x\n", ret);
	return ret;
}

int SOConnect(int snum, char *sock)
{
	//quit if no socket running or no input or input size not 8
	if(!socket_started || !sock || sock[0] != 8)
		return -1;
	int ret = _SOSockCmd(snum, sock, so_connect, 0);
	OSReport("SOConnect done %08x\n", ret);
	return ret;
}

int SOGetSockName(int snum, char *sock)
{
	//quit if no socket running or no input or input size not 8
	if(!socket_started || !sock || sock[0] != 8)
		return -1;
	int ret = _SOSockCmd(snum, sock, so_getsockname, 1);
	OSReport("SOGetSockName done %08x %08x %08x\n", ret, *(u32*)sock, *(u32*)(sock+4));
	return ret;
}

int SOGetPeerName(int snum, char *sock)
{
	//quit if no socket running or no input or input size not 8
	if(!socket_started || !sock || sock[0] != 8)
		return -1;
	int ret = _SOSockCmd(snum, sock, so_getpeername, 1);
	OSReport("SOGetPeerName done %08x %08x %08x\n", ret, *(u32*)sock, *(u32*)(sock+4));
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
	ret = doCall(so_shutdown, fd, &so_queue[fd]);
	freeUpFD(fd);
	OSReport("SOShutdown done %08x\n", ret);
	return ret;
}

int SORecvFrom(int snum, char *buf, int len, int flags, char *sock)
{
	if(!socket_started || !buf || len < 0 || (sock && sock[0] != 8))
		return -1;
	int ret;
	int fd = getFreeFD();
	//max len
	if(len >= (1<<(SOShift-1)))
		len = (1<<(SOShift-1));
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	//set common values
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = curbuf;
	sock_entry_arm[fd].cmd2 = len;
	sock_entry_arm[fd].cmd3 = flags;
	if(!sock)
	{
		sock_entry_arm[fd].cmd4 = 0;
		sock_entry_arm[fd].cmd5 = 0;
		ret = doCall(so_recvfrom, fd, &so_queue[fd]);
	}
	else
	{
		sock_entry_arm[fd].cmd4 = *(volatile u32*)(sock+0);
		sock_entry_arm[fd].cmd5 = *(volatile u32*)(sock+4);
		ret = doCall(so_recvfrom, fd, &so_queue[fd]);
		if(ret >= 0) //copy back sock
		{
			*(volatile u32*)(sock+0) = sock_entry_arm[fd].cmd4;
			*(volatile u32*)(sock+4) = sock_entry_arm[fd].cmd5;
		}
	}
	//copy back actually received data
	if(ret > 0)
	{
		sync_before_read((void*)curbuf, ret);
		int i;
		for(i = 0; i < ret; i++)
			*(volatile u8*)(buf+i) = *(volatile u8*)(curbuf+i);
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
	//max len
	if(len >= (1<<SOShift))
		len = (1<<SOShift);
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	//copy over data to send
	if(len > 0)
	{
		int i;
		for(i = 0; i < len; i++)
			*(volatile u8*)(curbuf+i) = *(volatile u8*)(buf+i);
		sync_after_write((void*)curbuf, len);
	}
	//set common values
	sock_entry_arm[fd].cmd0 = snum;
	sock_entry_arm[fd].cmd1 = curbuf;
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
	ret = doCall(so_sendto, fd, &so_queue[fd]);
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	sock_entry_arm[fd].cmd0 = curbuf;
	sock_entry_arm[fd].cmd1 = 0x24; //fixed out size
	*(volatile u32*)(curbuf+0x0) = snum;
	*(volatile u32*)(curbuf+0x4) = ctype;
	*(volatile u32*)(curbuf+0x8) = cmd;
	*(volatile u32*)(curbuf+0xC) = len;
	if(len)
	{
		int i;
		if(buf)
		{
			for(i = 0; i < len; i++)
				*(volatile u8*)(curbuf+0x10+i) = *(volatile u8*)(buf+i);
		}
		else
		{
			for(i = 0; i < len; i++)
				*(volatile u8*)(curbuf+0x10+i) = 0;
		}
	}
	sync_after_write((void*)curbuf, 0x24);
	ret = doCall(so_setsockopt, fd, &so_queue[fd]);
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
	ret = doCall(so_fcntl, fd, &so_queue[fd]);
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
	newpoll_params *newparams = (newpoll_params*)(SO_DATA_BUF+(fd<<SOShift));
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
	ret = doCall(so_poll, fd, &so_queue[fd]);
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	//do getmacaddr, note specifically for mario kart dd:
	//using an irq to wait for the result will crash for some reason,
	//possibly due to the specific game booting thread its executed from
	if(doSOGetInterfaceOpt(0x1004, (void*)curbuf, 6, fd, 0)) //read out from IOS address
	{
		*(volatile u32*)(retval+0) = *(volatile u32*)(curbuf+0);
		*(volatile u16*)(retval+4) = *(volatile u16*)(curbuf+4);
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	if(doSOGetInterfaceOpt(0x4003, (void*)curbuf, 12, fd, &so_queue[fd])) //read out from IOS address
		*retval = *(volatile u32*)(curbuf+8);
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
	OSReport("IPGetBroadcastAddr done %i.%i.%i.%i\n", ((*retval)>>24),((*retval)>>16)&0xFF,((*retval)>>8)&0xFF,(*retval)&0xFF);
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	if(doSOGetInterfaceOpt(0x4003, (void*)curbuf, 12, fd, &so_queue[fd])) //read out from IOS address
		*retval = *(volatile u32*)(curbuf+4);
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
	OSReport("IPGetNetmask done %i.%i.%i.%i\n", ((*retval)>>24),((*retval)>>16)&0xFF,((*retval)>>8)&0xFF,(*retval)&0xFF);
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	if(doSOGetInterfaceOpt(0x4003, (void*)curbuf, 12, fd, &so_queue[fd])) //read out from IOS address
		*retval = *(volatile u32*)curbuf;
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
	ip_cache = *retval;
	OSReport("IPGetAddr done %i.%i.%i.%i\n", ((*retval)>>24),((*retval)>>16)&0xFF,((*retval)>>8)&0xFF,(*retval)&0xFF);
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	if(doSOGetInterfaceOpt(0x4004, (void*)curbuf, 4, fd, &so_queue[fd])) //read out from IOS address
		*retval = *(volatile u32*)curbuf;
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	//do getlinkstate
	if(doSOGetInterfaceOpt(0x1005, (void*)curbuf, 4, fd, &so_queue[fd])) //read out from IOS address
		*retval = *(volatile u32*)curbuf;
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	//do getconfigerror
	if(doSOGetInterfaceOpt(0x1003, (void*)curbuf, 4, fd, &so_queue[fd])) //read out from IOS address
		ret = *(volatile u32*)curbuf;
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
		u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
		*(volatile u32*)curbuf = err;
		//do setconfigerror
		doSOSetInterfaceOpt(0x1003, (void*)curbuf, 4, fd, &so_queue[fd]);
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	//clear out current interface value
	*(volatile u32*)curbuf = 0;
	//do clearconfigerror
	doSOSetInterfaceOpt(0x1003, (void*)curbuf, 4, fd, &so_queue[fd]);
	freeUpFD(fd);
	conf_cache = ret;
	OSReport("IPClearConfigError done %08x\n", ret);
	return ret; //returns last interface value
}


/* All the PSO related hooks follow here */

typedef struct _threadCBDatStr {
	u32 cb;
	u32 buf;
	short len;
	short sock;
} threadCBDatStr;

typedef struct _myCtx {
	void *queue;
	u32 type;
} myCtx;

typedef struct _sendarr {
	u16 len;
	char *buf;
} sendarr;

enum {
	T_SEND = 0,
	T_RECV,
	T_ACPT,
	T_MAX
};

//game callback function definitions
typedef void(*send_cb)(int ret, int sock);
typedef void(*recv_cb)(int recvd, int sock);
typedef void(*acpt_cb)(int newsock, int sock, char *addr, u16 port);

//defined in linker script
extern int (*OSCreateThread)(void *thread, void *func, void *funcvar, void *stack_start, u32 stack_size, u32 prio, u32 params);
extern void (*OSResumeThread)(void *thread);
extern void setDHCPStateOffset(int status);

static volatile u32 avetcp_inited = 0;

static volatile u32 dns_arr[2];
static volatile u32 dns_num = 0;
static volatile char *dns_names[SO_TOTAL_FDS];
static volatile int dns_namelen[SO_TOTAL_FDS];

//send, receive and accept threads
static volatile u32 threadsRunning = 0;
static u8 ioThread[T_MAX][0x400] __attribute__((aligned(32)));
static u8 threadStack[T_MAX][0x1000] __attribute__((aligned(32)));
static volatile u8 threadLock[T_MAX];
static volatile u8 threadReq[T_MAX];
static volatile u8 threadReqFD[T_MAX];
static volatile u8 threadBusy[T_MAX][SO_TOTAL_FDS];
static threadCBDatStr threadReqDat[T_MAX][SO_TOTAL_FDS];
static myCtx threadCtx[T_MAX];

//new thread stuff
static int dbgSock = -1;
static volatile u8 threadInUse[T_MAX][SO_TOTAL_FDS];
static threadCBDatStr threadRequest[T_MAX][SO_TOTAL_FDS];

int lockThread(u32 num)
{
	int ret = 0;
	int val = disableIRQs();
	while(threadsRunning)
	{
		if(threadLock[num] == 0)
		{
			threadLock[num] = 1;
			ret = 1;
			break;
		}
		//thread busy, wait for it
		//to free up again
		restoreIRQs(val);
		u32 garbage = 0xFFFF;
		while(garbage--) ;
		val = disableIRQs();
	}
	restoreIRQs(val);
	return ret;
}

void unlockThread(u32 num)
{
	threadLock[num] = 0;
}


void test_debug(char *txt, int cnt, int p1, int p2, int p3){
#if 0
	if (dbgSock == -1){
		dbgSock = SOSocket(2,1,0);

		u32 garbage = 0xFFFF;
		while(garbage--) ;

		u32 input[2];
		input[0] = 0x08020000 | 0xc0c;
		input[1] = 0xc0a801c7;
		SOConnect(dbgSock,(char*)input);

		garbage = 0xFFFF;
		while(garbage--) ;
	}
	int i;
	char buf[128];
	for (i=0; i<128; i++) buf[i] = 0;
	*(volatile u32*)(buf+0) = cnt;
	*(volatile u32*)(buf+4) = p1;
	*(volatile u32*)(buf+8) = p2;
	*(volatile u32*)(buf+12) = p3;
	for (i=0; i<100; i++) if (txt[i] != 0) buf[i+16] = txt[i]; else break;
	SOSendTo(dbgSock, buf, 128, 0, 0);
#endif
}

int test_socket(int type, int sock){
	int i;
	for (i = 0; i<SO_TOTAL_FDS; i++)
		if (threadInUse[type][i] > 0)
			if (threadReqDat[type][i].sock == sock) return 0;
	return 1;
}

static void threadHandleNew(myCtx *ctx)
{
	int val = disableIRQs();
	void *queue = ctx->queue;
	u32 type = ctx->type;
	while(threadsRunning)
	{
		if(!threadsRunning) break;
		//no requests for now, so sleep
		OSSleepThread(queue);
		//had irq enabled so check
		if(!threadsRunning) break;
		//woke up, either request or callback


		int pool;
		int active = 1;
		//loop until all is done
		while (active){
			if(!threadsRunning) break;
			active = 0;
			//process any pending request to send
			for (pool = 0; pool < SO_TOTAL_FDS; pool++){
				if ((threadInUse[type][pool] & 6) == 2){
					active = 1; //still have stuff to do
					//this entry is to be processed
					u32 fd = pool;
					u32 buf = threadReqDat[type][fd].buf;
					short len = threadReqDat[type][fd].len;
					short sock = threadReqDat[type][fd].sock;

					if(type == T_SEND)
					{
						u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
						//copy over data to send
						if(len > 0)
						{
							int i;
							for(i = 0; i < len; i++)
								*(volatile u8*)(curbuf+i) = *(volatile u8*)(buf+i);
							sync_after_write((void*)curbuf, len);
						}
						sock_entry_arm[fd].cmd0 = sock;
						sock_entry_arm[fd].cmd1 = curbuf;
						sock_entry_arm[fd].cmd2 = len;
						sock_entry_arm[fd].cmd3 = 0;
						sock_entry_arm[fd].cmd4 = 0;
						sock_entry_arm[fd].cmd5 = 0;
						doCallStart(SO_THREAD_FD|so_sendto, fd, queue);
					}
					else if(type == T_RECV)
					{
						u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
						sock_entry_arm[fd].cmd0 = sock;
						sock_entry_arm[fd].cmd1 = curbuf;
						sock_entry_arm[fd].cmd2 = len;
						sock_entry_arm[fd].cmd3 = 0;
						sock_entry_arm[fd].cmd4 = 0;
						sock_entry_arm[fd].cmd5 = 0;
						doCallStart(SO_THREAD_FD|so_recvfrom, fd, queue);
					}
					else
					{
						sock_entry_arm[fd].cmd0 = sock;
						sock_entry_arm[fd].cmd1 = 0x08000000;
						sock_entry_arm[fd].cmd2 = 0;
						doCallStart(SO_THREAD_FD|so_accept, fd, queue);
					}
					threadInUse[type][pool] = 5;	//make it ready for pooling
				}
			}

			if(!threadsRunning) break;

			sync_before_read(sock_entry_arm, 0x40*0x20);
			//process any pending request to complete
			for (pool = 0; pool < SO_TOTAL_FDS; pool++){
				if ((threadInUse[type][pool] & 6) == 4){
					active = 1;
					//one call got back to us, handle it
					if(sock_entry_arm[pool].busy == 0)
					{
						//done call, get return value
						int ret = doCallEnd(pool);
						//do handling and cb with interrupts enabled
						restoreIRQs(val);
						u32 cb = threadReqDat[type][pool].cb;
						u32 sock = threadReqDat[type][pool].sock;
						if(type == T_SEND) {
							((send_cb)cb)(0, sock);
						}
						else if(type == T_RECV)
						{
							if(ret > 0)
							{
								u32 outbuf = threadReqDat[type][pool].buf;
								u32 curbuf = (SO_DATA_BUF+(pool<<SOShift));
								sync_before_read((void*)curbuf, ret);
								int c;
								for(c = 0; c < ret; c++)
									*(volatile u8*)(outbuf+c) = *(volatile u8*)(curbuf+c);
							}
							test_debug("async read",2,sock,ret,0);
							threadInUse[type][pool] = 0; //make it ok to use again
							((recv_cb)cb)(ret, sock);
						}
						else //accept
						{
							u32 buf[2];
							if(ret >= 0) //copy back return value
							{
								buf[0] = sock_entry_arm[pool].cmd1;
								buf[1] = sock_entry_arm[pool].cmd2;
							}
							u16 port = (u16)(buf[0]&0xFFFF);
							buf[0] = 4;
							threadInUse[type][pool] = 0;
							((acpt_cb)cb)(ret, sock, (char*)buf, port);
						}
						//cb done, disable irqs again
						val = disableIRQs();
						//done processing, free up fd
						
						freeUpFD(pool);
					}
				}
			}
			if(!threadsRunning) break;
		}
	}
}



int avetcp_init()
{
	SOInit();
	if(!avetcp_inited)
	{
		//start up io threads
		threadsRunning = 1;
		int i;
		for (i = 0; i < SO_TOTAL_FDS; i++)	{
			threadInUse[T_SEND][i] = 0;	//clear it for later
			threadInUse[T_RECV][i] = 0;
			threadInUse[T_ACPT][i] = 0;
		}
		so_queue[SO_THREAD_FD|so_sendto] = 0;
		threadCtx[T_SEND].queue = &so_queue[SO_THREAD_FD|so_sendto];
		so_queue[SO_THREAD_FD|so_recvfrom] = 0;
		threadCtx[T_RECV].queue = &so_queue[SO_THREAD_FD|so_recvfrom];
		so_queue[SO_THREAD_FD|so_accept] = 0;
		threadCtx[T_ACPT].queue = &so_queue[SO_THREAD_FD|so_accept];
		for(i = 0; i < T_MAX; i++)
		{
			threadCtx[i].type = i;
			OSCreateThread(ioThread[i], threadHandleNew, &threadCtx[i], threadStack[i] + 0x1000, 0x1000, 5, 0);
			OSResumeThread(ioThread[i]);
		}
		avetcp_inited = 1;
	}
	OSReport("avetcp_init done\n");
	return 0;
}

int avetcp_term()
{
	if(avetcp_inited)
	{
		lockThread(T_SEND);
		lockThread(T_RECV);
		lockThread(T_ACPT);
		threadsRunning = 0;
		OSWakeupThread(&so_queue[SO_THREAD_FD|so_sendto]);
		u32 garbage = 0xFFFFFF;
		while(garbage--) ;
		OSWakeupThread(&so_queue[SO_THREAD_FD|so_recvfrom]);
		garbage = 0xFFFFFF;
		while(garbage--) ;
		OSWakeupThread(&so_queue[SO_THREAD_FD|so_accept]);
		garbage = 0xFFFFFF;
		while(garbage--) ;
		unlockThread(T_SEND);
		unlockThread(T_RECV);
		unlockThread(T_ACPT);
		avetcp_inited = 0;
	}
	OSReport("avetcp_term done\n");
	return 0;
}

int dns_set_server(u32 *addr)
{
	u32 num = dns_num;
	OSReport("dns_set_server %i %i.%i.%i.%i\n", num, ((addr[1])>>24),((addr[1])>>16)&0xFF,((addr[1])>>8)&0xFF,(addr[1])&0xFF);
	if(addr[0] == 4 && addr[1]) dns_arr[num] = addr[1];
	//write to ios if at least primary is set
	if(dns_arr[0])
	{
		int fd = getFreeFD();
		u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
		*(volatile u32*)(curbuf+0) = dns_arr[0];
		*(volatile u32*)(curbuf+4) = dns_arr[1];
		//do set dns server
		doSOSetInterfaceOpt(0xB003, (void*)curbuf, 8, fd, &so_queue[fd]);
		freeUpFD(fd);
	}
	dns_num^=1;
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
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	int i;
	for(i = 0; i < dns_namelen[fd]; i++)
		*(volatile u8*)(curbuf+i) = dns_names[fd][i];
	sync_after_write((void*)curbuf, dns_namelen[fd]);
	sock_entry_arm[fd].cmd0 = curbuf;
	sock_entry_arm[fd].cmd1 = dns_namelen[fd];
	//giving it 0x200 for a name is overkill, should work fine
	sock_entry_arm[fd].cmd2 = curbuf+0x200;
	sock_entry_arm[fd].cmd3 = 0x460;
	int ret = doCall(so_gethostbyname, fd, &so_queue[fd]);
	if(ret >= 0)
	{
		sync_before_read((void*)curbuf+0x200, 0x460);
		u16 addrlen = *(u16*)(curbuf+0x20A);
		if(addrlen == 4)
		{
			//offset represents address difference between original IOS memory region and our memory region
			u32 ppc_offset = (curbuf+0x210) - *(u32*)(curbuf+0x200);
			//resolve ip entry from its various pointers
			u32 ip_arr_addr_ptr = (*(u32*)(curbuf+0x20C)) + ppc_offset;
			u32 ip_arr_entry0_ptr = (*(u32*)(ip_arr_addr_ptr)) + ppc_offset;
			u32 ip_arr_entry0 = *(u32*)ip_arr_entry0_ptr;
			//write resolved ip back into game
			arr[0] = addrlen;
			arr[1] = ip_arr_entry0;
			OSReport("dns_get_addr ret %i.%i.%i.%i\n", ((arr[1])>>24),((arr[1])>>16)&0xFF,((arr[1])>>8)&0xFF,(arr[1])&0xFF);
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
	test_debug("tcp_create",0,0,0,0);
	return SOSocket(2,1,0);
}

int tcp_bind(int sock, u32 *arr, u16 port)
{
	OSReport("tcp_bind %i %08x %i\n", sock, arr[1], sock);
	test_debug("tcp_bind",2,arr[1],port,0);
	u32 input[2];
	input[0] = 0x08020000 | port;
	input[1] = arr[1];
	return SOBind(sock,(char*)input);
}

int tcp_listen(int sock, u32 unk1, u32 unk2, int list, u32 unk3, u32 unk4)
{
	OSReport("tcp_listen %i %i\n", sock, list);
	test_debug("tcp_listen",0,0,0,0);
	return SOListen(sock, list);
}

int tcp_stat(int sock, short *status, u32 *unk1free, u32 *sendfree, u32 *dataAvaible)
{
	OSReport("tcp_stat %i\n", sock);
	test_debug("tcp_stat",0,0,0,0);
	//should find real value of the socket
	//this on init has to be 4 or more to work
	if(status) *status = 4; //AT_STAT_ESTABLISHED 
	//always say we have the full buffer to send
	if(unk1free) *unk1free = 0; //this is the amount of client connecting
	if(sendfree) *sendfree = (1<<(SOShift)); //max 32300 -- sending less than mtu to test
	if(dataAvaible) *dataAvaible = 512;	//max 10k, make it small for now to test as a max chunk
	return 0;
}

int tcp_getaddr(int sock, u32 *a1, u16 *p1, u32 *a2, u32 *p2)
{
	OSReport("tcp_getaddr %i\n", sock);
	test_debug("tcp_getaddr",0,0,0,0);
	//convert over socket address fist
	u32 tmpbuf[2];
	tmpbuf[0] = 0x08000000;
	tmpbuf[1] = 0;
	SOGetSockName(sock, (char*)tmpbuf);
	u16 port = (u16)(tmpbuf[0]&0xFFFF);
	a1[0] = 4;
	a1[1] = tmpbuf[1];
	*p1 = port;
	//now do connected peer address
	tmpbuf[0] = 0x08000000;
	tmpbuf[1] = 0;
	SOGetPeerName(sock, (char*)tmpbuf);
	port = (u16)(tmpbuf[0]&0xFFFF);
	a2[0] = 4;
	a2[1] = tmpbuf[1];
	*p2 = port;
	return 0;
}

int tcp_connect(int sock, u32 *arr, u16 port, u32 unk1)
{
	OSReport("tcp_connect %i %08x %i\n", sock, arr[1], sock);
	test_debug("tcp_connect",2,arr[0],port,0);
	u32 input[2];
	input[0] = 0x08020000 | port;
	input[1] = arr[1];
	return SOConnect(sock,(char*)input);
}

int tcp_accept(short sock, u32 cb, u32 bufmaybe)
{
	OSReport("tcp_accept %i\n", sock);
	test_debug("tcp_accept",0,0,0,0);
	if(cb)
	{
		if (test_socket(T_ACPT,sock)==0) return 0;	//already something pending
		if(!lockThread(T_ACPT)) return -1;
		int fd = getFreeFD();
		threadInUse[T_ACPT][fd] = 1;	//reserve it
		threadReqDat[T_ACPT][fd].cb = cb;
		threadReqDat[T_ACPT][fd].sock = sock;
		threadInUse[T_ACPT][fd] = 3; //ready to run
		OSWakeupThread(&so_queue[SO_THREAD_FD|so_accept]);
		unlockThread(T_ACPT);
		return -1;
	}
	//nothing uses bufmaybe, so not sure how to emulate,
	//just ignore and return socket fd alone for now
	return SOAccept(sock, (char*)0);
}

int tcp_send(int sock, u32 cb, int pcks, sendarr *arr)
{
	OSReport("tcp_send %i %08x %i %08x\n", sock, cb, pcks, (u32)arr);
	test_debug("tcp_send",2,sock,arr[0].len,0);
	if(cb) //only handling pcks=1 here, the only game using this does just that
	{
		if(!lockThread(T_SEND)) return -1;
		int fd = getFreeFD();
		threadInUse[T_SEND][fd] = 1;
		threadReqDat[T_SEND][fd].len = arr[0].len;
		threadReqDat[T_SEND][fd].buf = (u32)arr[0].buf;
		threadReqDat[T_SEND][fd].cb = cb;
		threadReqDat[T_SEND][fd].sock = sock;
		threadInUse[T_SEND][fd] = 3;
		OSWakeupThread(&so_queue[SO_THREAD_FD|so_sendto]);
		unlockThread(T_SEND);
		return 0;
	}
	int i;
	for(i = 0; i < pcks; i++){
		SOSendTo(sock, arr[i].buf, (arr[i].len), 0, 0);
	}
		
	return 0;
}

int tcp_receive(int sock, u32 cb, u32 len, char *buf)
{
	test_debug("tcp_receive req",2,sock,len,0);
	if (len > 1024*8) len = 1024*8;
	OSReport("tcp_receive %i %08x %i %08x\n", sock, cb, len, (u32)buf);
	
	if(cb)
	{
		if (test_socket(T_RECV,sock)==0) return 0;	//already something pending
		if(!lockThread(T_RECV)) return -1;
		int fd = getFreeFD();
		threadInUse[T_RECV][fd] = 1;
		threadReqDat[T_RECV][fd].len = len;
		threadReqDat[T_RECV][fd].buf = (u32)buf;
		threadReqDat[T_RECV][fd].cb = cb;
		threadReqDat[T_RECV][fd].sock = sock;
		threadInUse[T_RECV][fd] = 3;
		OSWakeupThread(&so_queue[SO_THREAD_FD|so_recvfrom]);
		unlockThread(T_RECV);
		return 0;
	}
	int ret = SORecvFrom(sock, buf, len, 0, 0);

	test_debug("tcp_receive read",2,sock,ret,0);
	return ret;
}

//just do the same for abort and delete for now,
//not sure if there even is much of a difference
int tcp_abort(int sock)
{
	OSReport("tcp_abort %i\n", sock);
	test_debug("tcp_abort",1,sock,0,0);
	SOShutdown(sock, 2);
	return SOClose(sock);
}
int tcp_delete(int sock)
{
	OSReport("tcp_delete %i\n", sock);
	test_debug("tcp_delete",1,sock,0,0);
	SOShutdown(sock, 2);
	return SOClose(sock);
}

int DHCP_request_nb(u32 unk1, u32 unk2, u32 unk3, u32 unk4, u32 *ip, u32 *netmask, u32 *bcast, u32 tries)
{
	OSReport("DHCP_request_nb\n");
	test_debug("DHCP_request_nb",0,0,0,0);
	SOStartup();
	*ip = 0;
	//should be more than enough retries
	int retries = 100;
	while(retries--)
	{
		u32 garbage = 0xFFFFFFF;
		while(garbage--) ;
		IPGetAddr(0, ip);
		if(*ip) break;
	}
	//timed out
	if(*ip == 0)
		return -1;
	//got ip, success
	IPGetNetmask(0, netmask);
	IPGetBroadcastAddr(0, bcast);
	dns_arr[0] = 0;
	dns_arr[1] = 0;
	dns_num = 0;
	setDHCPStateOffset(3); //done
	return 0;
}

int DHCP_get_gateway(u32 unk, u32 *addr)
{
	OSReport("DHCP_get_gateway\n");
	test_debug("DHCP_get_gateway",0,0,0,0);
	*addr = 0;
	//default interface request
	int fd = getFreeFD();
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	u32 entries = 0;
	if(doSOGetInterfaceOpt(0x4005, (void*)curbuf, 4, fd, &so_queue[fd])) //read out from IOS address
		entries = *(volatile u32*)curbuf;
	freeUpFD(fd);
	//no route entries
	if(!entries)
		return -1;
	//got entries, now parse it
	fd = getFreeFD();
	if(doSOGetInterfaceOpt(0x4006, (void*)curbuf, 24*entries, fd, &so_queue[fd])) //read out from IOS address
	{
		int i;
		for(i = 0; i < 24*entries; i+=24)
		{
			if(*(volatile u32*)(curbuf+i) == 0 && *(volatile u32*)(curbuf+i+4) == 0 && *(volatile u32*)(curbuf+i+8) != 0)
			{
				*addr = *(volatile u32*)(curbuf+i+8);
				OSReport("DHCP_get_gateway ret %i.%i.%i.%i\n", ((*addr)>>24),((*addr)>>16)&0xFF,((*addr)>>8)&0xFF,(*addr)&0xFF);
				break;
			}
		}
	}
	freeUpFD(fd);
	return 0;
}

int DHCP_get_dns(u32 unk, char *domain, u32 *addr1, u32 *addr2)
{
	OSReport("DHCP_get_dns\n");
	test_debug("DHCP_get_dns",0,0,0,0);
	//default interface request
	int fd = getFreeFD();
	u32 curbuf = (SO_DATA_BUF+(fd<<SOShift));
	if(doSOGetInterfaceOpt(0xB003, (void*)curbuf, 8, fd, &so_queue[fd])) //read out from IOS address
	{
		*addr1 = *(volatile u32*)(curbuf+0);
		*addr2 = *(volatile u32*)(curbuf+4);
		OSReport("DHCP_get_dns ret %i.%i.%i.%i and %i.%i.%i.%i\n", 
			((*addr1)>>24),((*addr1)>>16)&0xFF,((*addr1)>>8)&0xFF,(*addr1)&0xFF,
			((*addr2)>>24),((*addr2)>>16)&0xFF,((*addr2)>>8)&0xFF,(*addr2)&0xFF);
	}
	freeUpFD(fd);
	return 0;
}

int DHCP_release()
{
	OSReport("DHCP_release\n");
	test_debug("DHCP_release",0,0,0,0);
	setDHCPStateOffset(0); //off
	return SOCleanup();
}
