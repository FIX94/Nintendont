
#include "global.h"
#include "common.h"
#include "alloc.h"
#include "sock.h"
#include "string.h"
#include "debug.h"
#include "EXI.h"
extern vu32 TRIGame;
bool SO_IRQ = false;
extern bool EXI_IRQ;
int soIrqFd = 0;
static struct ipcmessage *sockmsg[5] = { NULL, NULL, NULL, NULL, NULL };

vu32 sockintr[5] = { 0, 0, 0, 0, 0 };
vs32 sockres[5] = { 0, 0, 0, 0, 0 };
int soBusy[5] = { 0, 0, 0, 0, 0 };
static s32 sockqueue = -1;

static u32 SOCKAlarm()
{
	struct ipcmessage *msg = NULL;
	while(1)
	{
		mqueue_recv(sockqueue, &msg, 0);
		int sock = msg->seek.origin;
		//dbgprintf("sock %08x msg\r\n",sock);
		sockres[sock] = msg->result;
		sockintr[sock] = 1;
		mqueue_ack(msg, 0);
	}
	return 0;
}

struct bind_params {
	u32 socket;
	u32 has_name;
	u8 name[28];
};

struct setsockopt_params {
	u32 socket;
	u32 level;
	u32 optname;
	u32 optlen;
	u8 optval[20];
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

struct bind_params *bindParams[5] = { NULL, NULL, NULL, NULL, NULL };
struct setsockopt_params *sockOptParams[5] = { NULL, NULL, NULL, NULL, NULL };
struct sendto_params *sendParams[5] = { NULL, NULL, NULL, NULL, NULL };
u32 *SOParams[5] = { NULL, NULL, NULL, NULL, NULL };
u32 *SOOutParams[5] = { NULL, NULL, NULL, NULL, NULL };
u32 *SOPollInParams[5] = { NULL, NULL, NULL, NULL, NULL };
struct oldpoll_params *SOPollOldOutParams[5] = { NULL, NULL, NULL, NULL, NULL };
struct newpoll_params *SOPollNewOutParams[5] = { NULL, NULL, NULL, NULL, NULL };
u8 *SOVecResA[5] = { NULL, NULL, NULL, NULL, NULL };
u8 *SOVecResB[5] = { NULL, NULL, NULL, NULL, NULL };
u8 *SOVecResC[5] = { NULL, NULL, NULL, NULL, NULL };
ioctlv *SOVec[5] = { NULL, NULL, NULL, NULL, NULL };
u8 *SODataBuf[5] = { NULL, NULL, NULL, NULL, NULL };

static u32 SO_Thread = 0;
u8 *soheap = NULL;
extern char __so_stack_addr, __so_stack_size;
int sockFd, ncdFd;
u32 sockTimer;
void SOCKInit()
{
	int i;
	for(i = 0; i < 5; i++) {
		bindParams[i] = (struct bind_params*)malloca(sizeof(struct bind_params),32);
		sockOptParams[i] = (struct setsockopt_params*)malloca(sizeof(struct setsockopt_params),32);
		sendParams[i] = (struct sendto_params*)malloca(sizeof(struct sendto_params),32);
		SOParams[i] = (u32*)malloca(32,32);
		SOOutParams[i] = (u32*)malloca(32,32);
		SOPollInParams[i] = (u32*)malloca(32,32);
		SOVecResA[i] = (u8*)malloca(32,32);
		SOVecResB[i] = (u8*)malloca(32,32);
		SOVecResC[i] = (u8*)malloca(32,32);
		SOVec[i] = (ioctlv*)malloca(sizeof(ioctlv)*3,32);
		sockmsg[i] = (struct ipcmessage*)malloca(sizeof(struct ipcmessage),32);
		memset(sockmsg[i], 0, sizeof(struct ipcmessage));
		SOPollOldOutParams[i] = (struct oldpoll_params*)malloca(sizeof(struct oldpoll_params)*16,32);
		SOPollNewOutParams[i] = (struct newpoll_params*)malloca(sizeof(struct newpoll_params)*16,32);
	}

	soheap = (u8*)malloca(32,32);
	sockqueue = mqueue_create(soheap, 1);

	SO_Thread = thread_create(SOCKAlarm, NULL, ((u32*)&__so_stack_addr), ((u32)(&__so_stack_size)) / sizeof(u32), 0x78, 1);
	thread_continue(SO_Thread);

	char *soDev = "/dev/net/ip/top";
	char *ncdDev = "/dev/net/ncd/manage";
	void *name = heap_alloc_aligned(0,32,32);
	memcpy(name,soDev,32);
	sockFd = IOS_Open(name,0);
	memcpy(name,ncdDev,32);
	ncdFd = IOS_Open(name,0);
	heap_free(0,name);

	mdelay(100);
	//dbgprintf("SOInit %08x\n", &soIf[1]);
}

void SOCKShutdown()
{
	IOS_Close(sockFd);
	IOS_Close(ncdFd);
}

typedef struct _getsockopt_params {
	u32 interface;
	u32 level;
	u32 optname;
	u8 optval[8];
	u32 optlen;
} getsockopt_params;

extern u32 CurrentTiming;
u32 SO_IRQ_Timer = 0;
bool SOCKCheckTimer(void)
{
	return 1;//TimerDiffTicks(SO_IRQ_Timer) > 8000;
}

void SOCKInterrupt(void)
{
	/*if(TRIGame != TRI_NONE)
	{
		write32( EXI_CAUSE_1, (1<<11) );
		sync_after_write( (void*)EXI_CAUSE_1, 0x20 );
	}
	else
	{
		write32( EXI_CAUSE_2, (1<<11) );
		sync_after_write( (void*)EXI_CAUSE_2, 0x20 );
	}*/
	//write32( EXI_CAUSE_0, (1<<11) );
	//sync_after_write( (void*)EXI_CAUSE_0, 0x20 );
	write32(EXI_INT, 0x2000); // test HSP IRQ
	write32(EXI_INT_FD, soIrqFd);
	sync_after_write( (void*)EXI_INT, 0x20 );
	write32( HW_IPC_ARMCTRL, (1<<0) | (1<<4) ); //throw irq
	dbgprintf("SOCK Interrupt %i\r\n", soIrqFd);
	SO_IRQ = false;
}

void SOCKUpdateRegisters()
{
	if(sockTimer && TimerDiffSeconds(sockTimer) >= 3)
	{
		sockTimer = 0;
		sync_before_read((void*)0x130269A0,0x20);
		write32(0x130269A0, read32(0x130269A4));
		sync_after_write((void*)0x130269A0,0x20);
	}
	if( EXI_IRQ == true || SO_IRQ == true || (read32(EXI_INT) & 0x2010) ) //still working
		return;
	int i, j;
	for(i = 0; i < 5; i++)
	{
		int bPos = (i<<5);
		sync_before_read((void*)(SO_BASE+bPos), 0x20);
		u32 ioctl = read32(SO_IOCTL+bPos);
		u32 optCmd, optLen, pollNum, dataLen;
		if(ioctl)
		{
			if(soBusy[i])
			{
				if(sockintr[i] == 0)
					continue;
				dbgprintf("result cmd for %i is %i\r\n",i,ioctl);
				if(ioctl & SO_NCD_CMD)
				{
					ioctl &= ~SO_NCD_CMD;
					switch(ioctl)
					{
						case 0x08:
							memcpy((void*)(SO_CMD_0+bPos),SOVecResB[i],6);
							break;
						default:
							break;
					}
				}
				else if(ioctl == 0x01)
				{
					write32(SO_CMD_1+bPos,SOOutParams[i][0]);
					write32(SO_CMD_2+bPos,SOOutParams[i][1]);
					write32(SO_RETVAL+bPos, sockres[i]);
					dbgprintf("Acceptres %08x\r\n",sockres[i]);
				}
				else if(ioctl == 0x0B)
				{
					pollNum = read32(SO_CMD_3+bPos);
					if(sockres[i] >= 0 && read32(SO_CMD_2+bPos) && pollNum)
					{
						for(j = 0; j < pollNum; j++) {
							if(((int)SOPollOldOutParams[i][j].socket) < 0) {
								pollNum = j;
								break;
							}
							//SOPollOldOutParams[i][j].socket = SOPollNewOutParams[i][j].socket;
							//SOPollOldOutParams[i][j].flagsIn = SOPollNewOutParams[i][j].flagsIn;
							SOPollOldOutParams[i][j].flagsOut = SOPollNewOutParams[i][j].flagsOut;
						}
						if(pollNum)
						{
							hexdump(SOPollOldOutParams[i], pollNum*sizeof(struct oldpoll_params));
							//memcpy((void*)P2C(read32(SO_CMD_2+bPos)), SOPollOldOutParams[i], pollNum*sizeof(struct oldpoll_params));
							sync_after_write(SOPollOldOutParams[i], pollNum*sizeof(struct oldpoll_params));
							write32(SO_CMD_1+bPos, ((u32)SOPollOldOutParams[i]) | 0xD0000000);
						}
						write32(SO_CMD_3+bPos, pollNum);
					}
					else
						write32(SO_CMD_3+bPos, 0);
					write32(SO_RETVAL+bPos, sockres[i]);
					dbgprintf("Pollres %08x\r\n",sockres[i]);
				}
				else if(ioctl == 0x0C)
				{
					if (read32(SO_CMD_4+bPos)) {
						memcpy((void*)(SO_CMD_2+bPos), SOVecResB[i], 8);
					}
					if(sockres[i] > 0) {
						write32(SO_CMD_0+bPos, ((u32)SODataBuf[i]) | 0xD0000000);
					}
					write32(SO_RETVAL+bPos, sockres[i]);
					dbgprintf("Recvres %08x\r\n",sockres[i]);
				}
				else if(ioctl == 0x1C)
				{
					if(sockres[i] >= 0) {
						vu32 len = *(vu32*)(SOVecResC[i]);
						//dbgprintf("sockres %i opt %i res %08x%08x\r\n",sockres[i],len,*(vu32*)SOVecResB[i],*(vu32*)(SOVecResB[i]+4));
						write32(SO_RETVAL+bPos, len);
						if(len > 0)
							memcpy((void*)(SO_CMD_0+bPos),SOVecResB[i],len);
					}
				}
				else if(ioctl == 0x1F)
				{
					dbgprintf("Startup %08x\r\n",sockres[i]);
					write32(SO_RETVAL+bPos, sockres[i]);
					if(sockres[i] >= 0) //5 seconds until link start
						sockTimer = read32(HW_TIMER);
				}
				else
				{
					//if(ioctl == 2 || ioctl == 3 || ioctl == 9 || ioctl == 0x0F || ioctl == 0x1F)
						dbgprintf("%08x\r\n",sockres[i]);
					write32(SO_RETVAL+bPos, sockres[i]);
				}
				/*if(ioctl == 0x0C || ioctl == 0x0D) {
					if(SODataBuf[i] != NULL) {
						free(SODataBuf[i]);
						SODataBuf[i] = NULL;
					}
				}*/			
				write32(SO_IOCTL+bPos, 0);
				sync_after_write((void*)(SO_BASE+bPos), 0x20);
				soBusy[i] = 0;
				sockintr[i] = 0;
				soIrqFd = i;
				SO_IRQ = true;
				SO_IRQ_Timer = read32(HW_TIMER);
			}
			else
			{
				dbgprintf("cmd for %i is %i\r\n",i,ioctl);
				if(ioctl & SO_NCD_CMD)
				{
					ioctl &= ~SO_NCD_CMD;
					switch(ioctl)
					{
						case 0x08:
							dbgprintf("NCD %08x\r\n", ioctl);
							memset(SOVec[i],0,sizeof(ioctlv)*3);
							memset(SOVecResA[i],0,0x20);
							memset(SOVecResB[i],0,0x20);
							SOVec[i][0].data = SOVecResA[i];
							SOVec[i][0].len = 0x20;
							SOVec[i][1].data = SOVecResB[i];
							SOVec[i][1].len = 0x06;
							sockmsg[i]->seek.origin = i;
							IOS_IoctlvAsync(ncdFd, ioctl, 0, 2, SOVec[i], sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						default:
							break;
					}
				}
				else
				{
					switch(ioctl)
					{
						case 0x01:
							SOParams[i][0] = read32(SO_CMD_0+bPos);
							sync_after_write(SOParams[i],0x20);
							SOOutParams[i][0] = read32(SO_CMD_1+bPos);
							SOOutParams[i][1] = read32(SO_CMD_2+bPos);
							sync_after_write(SOOutParams[i],0x20);
							dbgprintf("SOAccept %i %08x %i\r\n",read32(SO_CMD_0+bPos),read32(SO_CMD_1+bPos), read32(SO_CMD_2+bPos));
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 4, SOOutParams[i], 8, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x02:
							memset(bindParams[i], 0, sizeof(struct bind_params));
							bindParams[i]->socket = read32(SO_CMD_0+bPos);
							bindParams[i]->has_name = 1;
							memcpy(bindParams[i]->name, (void*)(SO_CMD_1+bPos), 8);
							sync_after_write(bindParams[i],0x40);
							dbgprintf("SOBind %i %08x %i\r\n",read32(SO_CMD_0+bPos),read32(SO_CMD_1+bPos),read32(SO_CMD_2+bPos));
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, bindParams[i], sizeof(struct bind_params), NULL, 0, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x03:
							SOParams[i][0] = read32(SO_CMD_0+bPos);
							sync_after_write(SOParams[i],0x20);
							dbgprintf("SOClose %i\r\n",read32(SO_CMD_0+bPos));
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 4, NULL, 0, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x05:
							SOParams[i][0] = read32(SO_CMD_0+bPos);
							SOParams[i][1] = read32(SO_CMD_1+bPos);
							SOParams[i][2] = read32(SO_CMD_2+bPos);
							sync_after_write(SOParams[i],0x20);
							dbgprintf("SOFcntl %i %i %i\r\n",read32(SO_CMD_0+bPos),read32(SO_CMD_1+bPos),read32(SO_CMD_2+bPos));
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 12, NULL, 0, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x09:
							memset(sockOptParams[i], 0, sizeof(struct setsockopt_params));
							sockOptParams[i]->socket = read32(SO_CMD_0+bPos);
							sockOptParams[i]->level = read32(SO_CMD_1+bPos);
							sockOptParams[i]->optname = read32(SO_CMD_2+bPos);
							sockOptParams[i]->optlen = read32(SO_CMD_4+bPos);
							dbgprintf("SOSetSockOpt %i %i %i %i\r\n",read32(SO_CMD_0+bPos),read32(SO_CMD_1+bPos),read32(SO_CMD_2+bPos),read32(SO_CMD_4+bPos));
							if(read32(SO_CMD_3+bPos) && read32(SO_CMD_4+bPos))
							{
								sync_before_read_align32((void*)P2C(read32(SO_CMD_3+bPos)), sockOptParams[i]->optlen);
								memcpy(sockOptParams[i]->optval, (void*)P2C(read32(SO_CMD_3+bPos)), sockOptParams[i]->optlen);
								hexdump(sockOptParams[i]->optval, sockOptParams[i]->optlen);
							}
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, sockOptParams[i], sizeof(struct setsockopt_params), NULL, 0, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x0A:
							SOParams[i][0] = read32(SO_CMD_0+bPos);
							SOParams[i][1] = read32(SO_CMD_1+bPos);
							sync_after_write(SOParams[i],0x20);
							dbgprintf("SOListen %i %i\r\n",read32(SO_CMD_0+bPos),read32(SO_CMD_1+bPos));
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 8, NULL, 0, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x0B:
							SOPollInParams[i][0] = 0;
							SOPollInParams[i][1] = (u32)(read64(SO_CMD_0+bPos) / 60750ULL);
							pollNum = read32(SO_CMD_3+bPos);
							dbgprintf("SOPoll %i %08x %i\r\n",SOPollInParams[i][1],read32(SO_CMD_2+bPos),pollNum);
							if(read32(SO_CMD_2+bPos) && pollNum)
							{
								sync_before_read_align32((void*)P2C(read32(SO_CMD_2+bPos)), pollNum*sizeof(struct oldpoll_params));
								memcpy(SOPollOldOutParams[i], (void*)P2C(read32(SO_CMD_2+bPos)), pollNum*sizeof(struct oldpoll_params));
								for(j = 0; j < pollNum; j++) {
									SOPollNewOutParams[i][j].socket = SOPollOldOutParams[i][j].socket;
									SOPollNewOutParams[i][j].flagsIn = SOPollOldOutParams[i][j].flagsIn;
									//SOPollNewOutParams[i][j].flagsOut = SOPollOldOutParams[i][j].flagsOut;
									SOPollNewOutParams[i][j].flagsOut = 0;
								}
								hexdump(SOPollNewOutParams[i], pollNum*sizeof(struct newpoll_params));
								sync_after_write(SOPollNewOutParams[i], pollNum*sizeof(struct newpoll_params));
							}
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, SOPollInParams[i], 8, SOPollNewOutParams[i], pollNum*sizeof(struct newpoll_params), sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x0C:
							memset(SOVec[i],0,sizeof(ioctlv)*3);
							memset(SOVecResB[i],0,8);
							*(vu32*)(SOVecResA[i]) = read32(SO_CMD_0+bPos);
							*(vu32*)(SOVecResA[i]+4) = read32(SO_CMD_3+bPos);
							SOVec[i][0].data = SOVecResA[i];
							SOVec[i][0].len = 8;
							sync_after_write(SOVecResA[i],8);
							dataLen = read32(SO_CMD_2+bPos);
							if(SODataBuf[i] != NULL) free(SODataBuf[i]);
							SODataBuf[i] = malloca(dataLen,32);
							memset(SODataBuf[i],0,dataLen);
							sync_after_write(SODataBuf[i],dataLen);
							SOVec[i][1].data = SODataBuf[i];
							SOVec[i][1].len = dataLen;
							if (read32(SO_CMD_4+bPos)) {
								SOVec[i][2].data = SOVecResB[i];
								SOVec[i][2].len = 8;
								SOVecResB[i][0] = 0x08;
								sync_after_write(SOVecResB[i],8);
							} else {
								SOVec[i][2].data = NULL;
								SOVec[i][2].len = 0;
							}
							dbgprintf("SORecvFrom %i %08x %i\r\n", read32(SO_CMD_0+bPos), read32(SO_CMD_1+bPos), dataLen);
							sockmsg[i]->seek.origin = i;
							IOS_IoctlvAsync(sockFd, ioctl, 1, 2, SOVec[i], sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x0D:
							memset(SOVec[i],0,sizeof(ioctlv)*2);
							memset(sendParams[i],0,sizeof(struct sendto_params));
							dataLen = read32(SO_CMD_1+bPos);
							if(SODataBuf[i] != NULL) free(SODataBuf[i]);
							SODataBuf[i] = malloca(dataLen,32);
							sync_before_read_align32((void*)P2C(read32(SO_CMD_0+bPos)), dataLen);
							memcpy(SODataBuf[i], (void*)P2C(read32(SO_CMD_0+bPos)), dataLen);
							SOVec[i][0].data = SODataBuf[i];
							SOVec[i][0].len = dataLen;
							sync_after_write(SODataBuf[i],dataLen);
							sendParams[i]->socket = read32(SO_RETVAL+bPos);
							sendParams[i]->flags = read32(SO_CMD_2+bPos);
							if ((read32(SO_CMD_3+bPos) >> 24) == 8) {
								sendParams[i]->has_destaddr = 1;
								memcpy(sendParams[i]->destaddr, (void*)SO_CMD_3+bPos, 8);
								//hexdump(sendParams[i]->destaddr, 8);
							} else {
								sendParams[i]->has_destaddr = 0;
							}
							SOVec[i][1].data = sendParams[i];
							SOVec[i][1].len = sizeof(struct sendto_params);
							sync_after_write(sendParams[i],sizeof(struct sendto_params));
							dbgprintf("SOSendTo %i %i\r\n", sendParams[i]->socket, dataLen);
							sockmsg[i]->seek.origin = i;
							IOS_IoctlvAsync(sockFd, ioctl, 0, 2, SOVec[i], sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x0F:
							SOParams[i][0] = read32(SO_CMD_0+bPos);
							SOParams[i][1] = read32(SO_CMD_1+bPos);
							SOParams[i][2] = read32(SO_CMD_2+bPos);
							sync_after_write(SOParams[i],0x20);
							dbgprintf("SOSocket %i %i %i\r\n",read32(SO_CMD_0+bPos),read32(SO_CMD_1+bPos),read32(SO_CMD_2+bPos));
							sockmsg[i]->seek.origin = i;
							IOS_IoctlAsync(sockFd, ioctl, SOParams[i], 12, NULL, 0, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x1C:
							optCmd = read32(SO_CMD_0+bPos);
							optLen = read32(SO_CMD_1+bPos);
							dbgprintf("SOGetInterfaceOpt %i %i\r\n", optCmd, optLen);
							memset(SOVec[i],0,sizeof(ioctlv)*3);
							memset(SOVecResB[i],0,0x20);
							u32 level = 0xFFFE;
							*(vu32*)(SOVecResA[i]) = level;
							*(vu32*)(SOVecResA[i]+4) = optCmd;
							SOVec[i][0].data = SOVecResA[i];
							SOVec[i][0].len = 8;
							SOVec[i][1].data = SOVecResB[i];
							SOVec[i][1].len = optLen;
							*(vu32*)(SOVecResC[i]) = optLen;
							SOVec[i][2].data = SOVecResC[i];
							SOVec[i][2].len = 4;
							sync_after_write(SOVecResA[i],0x20);
							sync_after_write(SOVecResB[i],0x20);
							sync_after_write(SOVecResC[i],0x20);
							sockmsg[i]->seek.origin = i;
							IOS_IoctlvAsync(sockFd, ioctl, 1, 2, SOVec[i], sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0x1F:
							sockmsg[i]->seek.origin = i;
							dbgprintf("SOStartup\r\n");
							IOS_IoctlAsync(sockFd, ioctl, NULL, 0, NULL, 0, sockqueue, sockmsg[i]);
							soBusy[i] = 1;
							break;
						case 0xFF:
							dbgprintf("Ret Addr %08x\r\n",read32(SO_CMD_0+bPos));
							write32(SO_CMD_0+bPos, 0);
							write32(SO_IOCTL+bPos, 0);
							sync_after_write((void*)(SO_BASE+bPos), 0x20);
						default:
							break;
					}
				}
			}
			break;
		}
	}
}
