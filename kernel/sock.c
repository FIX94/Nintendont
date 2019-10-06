/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

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
#include "global.h"
#include "common.h"
#include "alloc.h"
#include "sock.h"
#include "string.h"
#include "EXI.h"

#ifndef DEBUG_SOCK
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

int soHeap = -1;
//total overall allocated
#define SO_TOTAL_FDS 64
//total depending on game
vu16 SOCurrentTotalFDs = SO_TOTAL_FDS;

static volatile unsigned char *ioctl_ppc = (unsigned char*)(SO_STRUCT_BUF+0x00);
static volatile unsigned char *ioctl_arm = (unsigned char*)(SO_STRUCT_BUF+0x40);
static so_struct_arm *sock_entry_arm = (so_struct_arm*)(SO_STRUCT_BUF+0x80);

static struct ipcmessage *sockmsg[SO_TOTAL_FDS];

static vu8 soBusy[SO_TOTAL_FDS];
static s32 sockqueue = -1;

static struct bind_params *bindParams[SO_TOTAL_FDS];
static struct sendto_params *sendParams[SO_TOTAL_FDS];
static u32 *SOParams[SO_TOTAL_FDS];
static u32 *SOOutParams[SO_TOTAL_FDS];
static u8 *SOVecResA[SO_TOTAL_FDS];
static u8 *SOVecResB[SO_TOTAL_FDS];
static u8 *SOVecResC[SO_TOTAL_FDS];
static ioctlv *SOVec[SO_TOTAL_FDS];

static u32 SO_Thread = 0;
static u8 *soqueueheap = NULL;
static u32 *so_stack;
static int sockFd, nwcFd;
static int ourCurrentIP;

static u32 SOCKAlarm()
{
	struct ipcmessage *msg = NULL;
	while(1)
	{
		mqueue_recv(sockqueue, &msg, 0);
		int i = msg->seek.origin;
		//dbgprintf("sock %08x msg\r\n",i);
		int res = msg->result;
		mqueue_ack(msg, 0);
		u32 ioctl = ioctl_arm[i];
		//dbgprintf("result cmd for %i ioctl %08x\r\n",i,ioctl);
		switch(ioctl&~SO_THREAD_FD)
		{
			case so_accept:
			case so_getpeername:
			case so_getsockname:
				sock_entry_arm[i].retval = res;
				if(res >= 0)
				{
					//copy back result
					if((sock_entry_arm[i].cmd1>>24) == 8)
					{
						memcpy((void*)&sock_entry_arm[i].cmd1, SOOutParams[i], 8);
						//dbgprintf("[%i] IOCTL %08x res %08x %08x%08x\r\n",i,ioctl,res,sock_entry_arm[i].cmd1,sock_entry_arm[i].cmd2);
					}
				}
				break;
			case so_poll:
				sock_entry_arm[i].retval = res;
				if(res >= 0)
				{
					sync_after_write((void*)SOOutParams[i][0], SOOutParams[i][1]);
					//dbgprintf("[%i] Poll res %08x\r\n",i,res);
					#ifdef DEBUG_SOCK
					//hexdump((void*)SOOutParams[i][0], SOOutParams[i][1]);
					#endif
				}
				break;
			case so_recvfrom:
				sock_entry_arm[i].retval = res;
				if(res >= 0)
				{
					sync_after_write(SOVec[i][1].data, SOVec[i][1].len);
					//copy back result
					if((sock_entry_arm[i].cmd4>>24) == 8)
					{
						memcpy((void*)&sock_entry_arm[i].cmd4, SOVecResB[i], 8);
						//FOR OUR BROADCAST HACK: Prevent loopback
						if(sock_entry_arm[i].cmd5 == ourCurrentIP)
						{
							//dbgprintf("[%i] Broadcast Hack: Recv blocking loopback\n", i);
							sock_entry_arm[i].retval = 0;
						}
					}
					//dbgprintf("[%i] Recv res %08x %08x%08x\r\n",i,res,*(vu32*)(SOVecResB[i]),*(vu32*)(SOVecResB[i]+4));
					#ifdef DEBUG_SOCK
					//hexdump(SOVec[i][1].data, res);
					#endif
				}
				break;
			case so_getinterfaceopt:
				sock_entry_arm[i].retval = res;
				if(res >= 0)
				{
					sync_after_write(SOVec[i][1].data, SOVec[i][1].len);
					//dbgprintf("[%i] GetInterfaceOpt %i opt %08x len %i res %08x%08x\r\n",i,res, 
					//	read32((u32)SOVecResA[i]+4), read32((u32)SOVecResC[i]), read32((u32)SOVec[i][1].data), read32((u32)SOVec[i][1].data+4));
					//FOR OUR BROADCAST HACK: Get comparison IP
					if(read32((u32)SOVecResA[i]+4) == 0x4003 && read32((u32)SOVec[i][1].data) != 0)
						ourCurrentIP = read32((u32)SOVec[i][1].data);
				}
				break;
			case nwc_startup:
				sock_entry_arm[i].retval = SOOutParams[i][0];
				break;
			case nwc_cleanup:
				sock_entry_arm[i].retval = SOOutParams[i][0];
				//FOR OUR BROADCAST HACK: Reset comparison IP
				ourCurrentIP = 0;
				break;
			//all the other SO commands
			case so_bind:
			case so_close:
			case so_connect:
			case so_fcntl:
			case so_setsockopt:
			case so_listen:
			case so_sendto:
			case so_shutdown:
			case so_socket:
			case so_gethostbyname:
			case so_setinterfaceopt:
			case so_startinterface:
				sock_entry_arm[i].retval = res;
				break;
			default: //should never happen
				Shutdown();
				break;
		}
		//if(sock_entry_arm[i].retval < 0)
		//	dbgprintf("[%i] WARNING: Ret for ioctl %08x was %08x\r\n", i, ioctl, sock_entry_arm[i].retval);
		sock_entry_arm[i].busy = 0;
		sync_after_write(&sock_entry_arm[i], 0x20);
		soBusy[i] = 0;
		write32(HSP_INT, 0x2000 | ((ioctl&SO_THREAD_FD) ? ioctl : i)); // HSP IRQ
		sync_after_write( (void*)HSP_INT, 0x20 );
		//int rep = 0;
		do {
			//rep++;
			//dbgprintf("%i\r\n", rep);
			write32(HW_IPC_ARMCTRL, 8); //throw irq
			udelay(10); //just waits some short time before checking
			sync_before_read((void*)HSP_INT, 0x20);
		} while(read32(HSP_INT) & 0x2000); //still working
		//if(rep > 1)
		//	dbgprintf("%i\r\n", rep);
	}
	return 0;
}

struct bind_params {
	u32 socket;
	u32 has_name;
	u8 name[28];
};

struct sendto_params {
	u32 socket;
	u32 flags;
	u32 has_destaddr;
	u8 destaddr[28];
};

struct oldpoll_params {
	s32 socket;
	s16 flagsIn;
	s16 flagsOut;
};

struct newpoll_params {
	s32 socket;
	s32 flagsIn;
	s32 flagsOut;
};

void SOCKInit()
{
	//give us 128KB, should be plenty
	soHeap = heap_create((void*)0x13040000, 0x20000);
	//dbgprintf("%08x\n", soHeap);
	int i;
	for(i = 0; i < SO_TOTAL_FDS; i++)
	{
		bindParams[i] = (struct bind_params*)heap_alloc_aligned(soHeap, sizeof(struct bind_params), 32);
		sendParams[i] = (struct sendto_params*)heap_alloc_aligned(soHeap, sizeof(struct sendto_params), 32);
		SOParams[i] = (u32*)heap_alloc_aligned(soHeap, 32, 32);
		SOOutParams[i] = (u32*)heap_alloc_aligned(soHeap, 32, 32);
		SOVecResA[i] = (u8*)heap_alloc_aligned(soHeap, 32, 32);
		SOVecResB[i] = (u8*)heap_alloc_aligned(soHeap, 32, 32);
		SOVecResC[i] = (u8*)heap_alloc_aligned(soHeap, 32, 32);
		SOVec[i] = (ioctlv*)heap_alloc_aligned(soHeap, sizeof(ioctlv)*3, 32);
		sockmsg[i] = (struct ipcmessage*)heap_alloc_aligned(soHeap, sizeof(struct ipcmessage), 32);
		memset(sockmsg[i], 0, sizeof(struct ipcmessage));
	}

	soqueueheap = (u8*)heap_alloc_aligned(soHeap, 0x200, 32);
	sockqueue = mqueue_create(soqueueheap, SO_TOTAL_FDS);

	so_stack = (u32*)heap_alloc_aligned(soHeap, 0x400, 32);
	SO_Thread = thread_create(SOCKAlarm, NULL, so_stack, 0x400 / sizeof(u32), 0x78, 1);
	thread_continue(SO_Thread);

	char *soDev = "/dev/net/ip/top";
	char *nwcDev = "/dev/net/kd/request";
	void *name = heap_alloc_aligned(soHeap,32,32);
	memcpy(name, soDev, 32);
	sockFd = IOS_Open(name, 0);
	memcpy(name, nwcDev, 32);
	nwcFd = IOS_Open(name, 0);
	heap_free(soHeap, name);

	memset((void*)ioctl_ppc, 0, 0x40);
	sync_after_write((void*)ioctl_ppc, 0x40);
	memset((void*)ioctl_arm, 0, 0x40);
	sync_after_write((void*)ioctl_arm, 0x40);

	ourCurrentIP = 0;
	//mdelay(100);
	//dbgprintf("SOInit %08x\n", &soIf[1]);
}

void SOCKShutdown()
{
	IOS_Close(sockFd);
	IOS_Close(nwcFd);
	heap_destroy(soHeap);
	soHeap = -1;
}

void SOCKUpdateRegisters()
{
	int i;
	sync_before_read((void*)ioctl_ppc, 0x40);
	for(i = 0; i < SOCurrentTotalFDs; i++)
	{
		if(ioctl_ppc[i] == 0) //no cmd on this fd
		{
			ioctl_arm[i] = 0;
			continue;
		}
		else if(soBusy[i]) //processing right now
			continue;
		else if(ioctl_arm[i]) //previously in use, wait for ppc clear
			continue;
		//new command, set it
		u32 ioctl = ioctl_arm[i] = ioctl_ppc[i];
		soBusy[i] = 1;
		sockmsg[i]->seek.origin = i;
		sync_before_read(&sock_entry_arm[i], 0x20);
		u32 optCmd, optLen;
		u8 *dataBuf;
		u64 time;
		ioctl&=~SO_THREAD_FD;
		//dbgprintf("cmd for %i ioctl %08x\r\n",i,ioctl);
		switch(ioctl)
		{
			case so_accept:
				SOParams[i][0] = sock_entry_arm[i].cmd0;
				if((sock_entry_arm[i].cmd1>>24) == 8)
				{
					memcpy(SOOutParams[i], (void*)&sock_entry_arm[i].cmd1, 8);
					dbgprintf("[%i] SOAccept %i %08x%08x\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1, sock_entry_arm[i].cmd2);
					IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 4, SOOutParams[i], 8, sockqueue, sockmsg[i]);
				}
				else //no return requested
				{
					dbgprintf("[%i] SOAccept %i\r\n", i,sock_entry_arm[i].cmd0);
					IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 4, NULL, 0, sockqueue, sockmsg[i]);
				}
				break;
			case so_bind:
				memset(bindParams[i], 0, sizeof(struct bind_params));
				bindParams[i]->socket = sock_entry_arm[i].cmd0;
				bindParams[i]->has_name = 1;
				memcpy(bindParams[i]->name, (void*)&sock_entry_arm[i].cmd1, 8);
				dbgprintf("[%i] SOBind %i %08x%08x\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1,sock_entry_arm[i].cmd2);
				IOS_IoctlAsync(sockFd, ioctl, bindParams[i], sizeof(struct bind_params), NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_close:
				SOParams[i][0] = sock_entry_arm[i].cmd0;
				dbgprintf("[%i] SOClose %i\r\n", i,sock_entry_arm[i].cmd0);
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 4, NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_connect:
				memset(bindParams[i], 0, sizeof(struct bind_params));
				bindParams[i]->socket = sock_entry_arm[i].cmd0;
				bindParams[i]->has_name = 1;
				memcpy(bindParams[i]->name, (void*)&sock_entry_arm[i].cmd1, 8);
				dbgprintf("[%i] SOConnect %i %08x%08x\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1,sock_entry_arm[i].cmd2);
				IOS_IoctlAsync(sockFd, ioctl, bindParams[i], sizeof(struct bind_params), NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_fcntl:
				SOParams[i][0] = sock_entry_arm[i].cmd0;
				SOParams[i][1] = sock_entry_arm[i].cmd1;
				SOParams[i][2] = sock_entry_arm[i].cmd2;
				sync_after_write(SOParams[i],0x20);
				dbgprintf("[%i] SOFcntl %i %i %i\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1,sock_entry_arm[i].cmd2);
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 12, NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_getpeername:
				SOParams[i][0] = sock_entry_arm[i].cmd0;
				SOOutParams[i][0] = sock_entry_arm[i].cmd1;
				SOOutParams[i][1] = sock_entry_arm[i].cmd2;
				dbgprintf("[%i] SOGetPeerName %i %08x%08x\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1,sock_entry_arm[i].cmd2);
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 4, SOOutParams[i], 8, sockqueue, sockmsg[i]);
				break;
			case so_getsockname:
				SOParams[i][0] = sock_entry_arm[i].cmd0;
				SOOutParams[i][0] = sock_entry_arm[i].cmd1;
				SOOutParams[i][1] = sock_entry_arm[i].cmd2;
				dbgprintf("[%i] SOGetSockName %i %08x%08x\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1,sock_entry_arm[i].cmd2);
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 4, SOOutParams[i], 8, sockqueue, sockmsg[i]);
				break;
			case so_setsockopt:
				dataBuf = (void*)(sock_entry_arm[i].cmd0&(~(1<<31)));
				optLen = sock_entry_arm[i].cmd1;
				sync_before_read(dataBuf, optLen);
				dbgprintf("[%i] SOSetSockOpt %08x %08x %08x %08x%08x\r\n", i,
					read32(((u32)dataBuf)+0x4),read32(((u32)dataBuf)+0x8),read32(((u32)dataBuf)+0xC),read32(((u32)dataBuf)+0x10),read32(((u32)dataBuf)+0x14));
				IOS_IoctlAsync(sockFd, ioctl, dataBuf, optLen, NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_listen:
				memcpy(SOParams[i], (void*)&sock_entry_arm[i].cmd0, 8);
				dbgprintf("[%i] SOListen %i %i\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1);
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 8, NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_poll:
				time = (read64((u32)(&sock_entry_arm[i].cmd2)) / 60750ULL);
				SOParams[i][0] = (u32)(time>>32);
				SOParams[i][1] = (u32)(time&0xFFFF);
				SOOutParams[i][0] = sock_entry_arm[i].cmd0&(~(1<<31));
				SOOutParams[i][1] = sock_entry_arm[i].cmd1;
				sync_before_read((void*)SOOutParams[i][0], SOOutParams[i][1]);
				dbgprintf("[%i] SOPoll %i %i\r\n", i,SOParams[i][1],SOOutParams[i][1]);
				#ifdef DEBUG_SOCK
				//hexdump((void*)SOOutParams[i][0], SOOutParams[i][1]);
				#endif
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 8, (void*)SOOutParams[i][0], SOOutParams[i][1], sockqueue, sockmsg[i]);
				break;
			case so_recvfrom:
				memset(SOVec[i],0,sizeof(ioctlv)*3);
				*(vu32*)(SOVecResA[i]) = sock_entry_arm[i].cmd0;
				*(vu32*)(SOVecResA[i]+4) = sock_entry_arm[i].cmd3;
				SOVec[i][0].data = SOVecResA[i];
				SOVec[i][0].len = 8;
				SOVec[i][1].data = (void*)(sock_entry_arm[i].cmd1&(~(1<<31)));
				SOVec[i][1].len = sock_entry_arm[i].cmd2;
				if((sock_entry_arm[i].cmd4>>24) == 8)
				{
					memcpy(SOVecResB[i], (void*)&sock_entry_arm[i].cmd4, 8);
					SOVec[i][2].data = SOVecResB[i];
					SOVec[i][2].len = 8;
				}
				else
				{
					SOVec[i][2].data = NULL;
					SOVec[i][2].len = 0;
				}
				dbgprintf("[%i] SORecvFrom %i %08x %i\r\n", i, sock_entry_arm[i].cmd0, sock_entry_arm[i].cmd1, sock_entry_arm[i].cmd2);
				IOS_IoctlvAsync(sockFd, ioctl, 1, 2, SOVec[i], sockqueue, sockmsg[i]);
				break;
			case so_sendto:
				SOVec[i][0].data = (void*)(sock_entry_arm[i].cmd1&(~(1<<31)));
				SOVec[i][0].len = sock_entry_arm[i].cmd2;
				sync_before_read(SOVec[i][0].data, SOVec[i][0].len);
				sendParams[i]->socket = sock_entry_arm[i].cmd0;
				sendParams[i]->flags = sock_entry_arm[i].cmd3;
				if((sock_entry_arm[i].cmd4>>24) == 8)
				{
					//FOR OUR BROADCAST HACK: Redirect sends from Multicast IP to Broadcast IP
					if((sock_entry_arm[i].cmd4&0xFFFF) == 1900 && sock_entry_arm[i].cmd5 == 0xEFFFFFFA)
					{
						//dbgprintf("[%i] Broadcast Hack: Redirecting Send to Broadcast IP\n", i);
						sock_entry_arm[i].cmd5 = 0xFFFFFFFF;
					}
					sendParams[i]->has_destaddr = 1;
				}
				else //size not 8, so no dest address given
					sendParams[i]->has_destaddr = 0;
				memcpy(sendParams[i]->destaddr, (void*)&sock_entry_arm[i].cmd4, 8);
				SOVec[i][1].data = sendParams[i];
				SOVec[i][1].len = sizeof(struct sendto_params);
				dbgprintf("[%i] SOSendTo %i %i %i %08x%08x\r\n", i, sendParams[i]->socket, SOVec[i][0].len, sendParams[i]->has_destaddr,
					sock_entry_arm[i].cmd4, sock_entry_arm[i].cmd5);
				#ifdef DEBUG_SOCK
				//hexdump(SOVec[i][0].data, SOVec[i][0].len);
				#endif
				IOS_IoctlvAsync(sockFd, ioctl, 2, 0, SOVec[i], sockqueue, sockmsg[i]);
				break;
			case so_shutdown:
				memcpy(SOParams[i], (void*)&sock_entry_arm[i].cmd0, 8);
				dbgprintf("[%i] SOShutdown %i %i\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1);
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 8, NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_socket:
				memcpy(SOParams[i], (void*)&sock_entry_arm[i].cmd0, 12);
				dbgprintf("[%i] SOSocket %i %i %i\r\n", i,sock_entry_arm[i].cmd0,sock_entry_arm[i].cmd1,sock_entry_arm[i].cmd2);
				IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 12, NULL, 0, sockqueue, sockmsg[i]);
				break;
			case so_gethostbyname:
				dbgprintf("[%i] SOGetHostByName %s %i\r\n", i,(char*)(sock_entry_arm[i].cmd0&(~(1<<31))),sock_entry_arm[i].cmd1);
				IOS_IoctlAsync(sockFd, ioctl, (void*)(sock_entry_arm[i].cmd0&(~(1<<31))), sock_entry_arm[i].cmd1, 
											(void*)(sock_entry_arm[i].cmd2&(~(1<<31))), sock_entry_arm[i].cmd3, sockqueue, sockmsg[i]);
				break;
			case so_getinterfaceopt:
				optCmd = sock_entry_arm[i].cmd0;
				optLen = sock_entry_arm[i].cmd2;
				//if(optCmd != 0x1003)
				dbgprintf("[%i] SOGetInterfaceOpt %08x %i\r\n", i, optCmd, optLen);
				write32((u32)SOVecResA[i], 0xFFFE);
				write32((u32)SOVecResA[i]+4, optCmd);
				SOVec[i][0].data = SOVecResA[i];
				SOVec[i][0].len = 8;
				SOVec[i][1].data = (void*)(sock_entry_arm[i].cmd1&(~(1<<31)));
				SOVec[i][1].len = optLen;
				memset(SOVec[i][1].data, 0, SOVec[i][1].len);
				write32((u32)SOVecResC[i], optLen);
				SOVec[i][2].data = SOVecResC[i];
				SOVec[i][2].len = 4;
				IOS_IoctlvAsync(sockFd, ioctl, 1, 2, SOVec[i], sockqueue, sockmsg[i]);
				break;
			case so_setinterfaceopt:
				optCmd = sock_entry_arm[i].cmd0;
				optLen = sock_entry_arm[i].cmd2;
				dbgprintf("[%i] SOSetInterfaceOpt %08x %i\r\n", i, optCmd, optLen);
				write32((u32)SOVecResA[i], 0xFFFE);
				write32((u32)SOVecResA[i]+4, optCmd);
				SOVec[i][0].data = SOVecResA[i];
				SOVec[i][0].len = 8;
				SOVec[i][1].data = (void*)(sock_entry_arm[i].cmd1&(~(1<<31)));
				SOVec[i][1].len = optLen;
				sync_before_read(SOVec[i][1].data, SOVec[i][1].len);
				IOS_IoctlvAsync(sockFd, ioctl, 2, 0, SOVec[i], sockqueue, sockmsg[i]);
				break;
			case so_startinterface:
				dbgprintf("[%i] SOStartInterface\r\n", i);
				IOS_IoctlAsync(sockFd, ioctl, NULL, 0, NULL, 0, sockqueue, sockmsg[i]);
				break;
			case nwc_startup:
			case nwc_cleanup:
				ioctl &= ~SO_NWC_CMD;
				dbgprintf("[%i] NWC %08x\r\n", i, ioctl);
				SOOutParams[i][0] = -1;
				IOS_IoctlAsync(nwcFd, ioctl, NULL, 0, SOOutParams[i], 0x20, sockqueue, sockmsg[i]);
				break;
			default: //should never happen
				Shutdown();
				break;
		}
	}
	sync_after_write((void*)ioctl_arm, 0x40);
}
