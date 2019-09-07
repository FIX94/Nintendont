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

static u32 socket_inited = 0;
static u32 socket_started = 0;
static u32 interface_started = 0;
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

#ifndef DEBUG_SOCK
#define OSReport(...)
#else
//mario kart ntsc-u
//static void (*OSReport)(char *in, ...) = (void*)0x800E8078;
//kirby air ride ntsc-u
static void (*OSReport)(char *in, ...) = (void*)0x803D4CE8;
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
	if(socket_inited)
		return;
	sync_before_read((void*)SO_HSP_LOC, 0x20);
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
	socket_inited = 1;
	OSReport("SOInit done\n");
	restoreIRQs(val);
}

int doCall(int call, int fd, int doIRQ)
{
	OSReport("doCall start %i %i\n", call, fd);
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
	OSReport("doCall done %i\n", sock_entry_arm[fd].retval);
	return sock_entry_arm[fd].retval;
}

int doStartSOInterface(int fd, int doIRQ)
{
	if(!socket_inited) //no interface to call open
		return 0;
	if(!interface_started)
	{
		if(doCall(so_startinterface, fd, doIRQ) >= 0)
		{
			int val = disableIRQs();
			interface_started = 1;
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
	if(!socket_inited) //no interface to call open
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
	restoreIRQs(val);
	OSReport("SOStartup done\n");
	return 0;
}

int SOCleanup(void)
{
	if(!socket_inited) //no interface to call open
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
	//default interface request
	int fd = getFreeFD();
	if(doSOGetInterfaceOpt(0x4003, (void*)sock_data_buf[fd].buf, 12, fd, 1)) //read out from IOS address
		*retval = *(volatile u32*)sock_data_buf[fd].buf;
	else //just return 0
		*retval = 0;
	freeUpFD(fd);
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
	//default interface request
	int fd = getFreeFD();
	//do getlinkstate
	if(doSOGetInterfaceOpt(0x1005, (void*)sock_data_buf[fd].buf, 4, fd, 1)) //read out from IOS address
		*retval = *(volatile u32*)sock_data_buf[fd].buf;
	else //just say link down
		*retval = 0;
	freeUpFD(fd);
	OSReport("IPGetLinkState done %08x\n", *retval);
}

int IPGetConfigError(char *interface_in)
{
	if(interface_in)
		return *(volatile u32*)(interface_in+8);
	int ret;
	//default interface request
	int fd = getFreeFD();
	//do getconfigerror
	if(doSOGetInterfaceOpt(0x1003, (void*)sock_data_buf[fd].buf, 4, fd, 1)) //read out from IOS address
		ret = *(volatile u32*)sock_data_buf[fd].buf;
	else //return some negative number I guess
		ret = -1;
	freeUpFD(fd);
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
	//default interface request
	int fd = getFreeFD();
	//clear out current interface value
	*(volatile u32*)sock_data_buf[fd].buf = 0;
	//do clearconfigerror
	doSOSetInterfaceOpt(0x1003, (void*)sock_data_buf[fd].buf, 4, fd, 1);
	freeUpFD(fd);
	OSReport("IPClearConfigError done %08x\n", ret);
	return ret; //returns last interface value
}
