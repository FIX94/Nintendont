#include "global.h"
#include "common.h"
#include "string.h"
#include "net.h"

#define INADDR_ANY 0
#define IPPROTO_IP 0
#define SOCK_STREAM 1
#define AF_INET 2

struct address {
	unsigned char len;
	unsigned char family;
	unsigned short port;
	unsigned int name;
	unsigned char unused[20];
};

struct bind_params {
	unsigned int socket;
	unsigned int has_name;
	struct address addr;
};

struct sendto_params {
	unsigned int socket;
	unsigned int flags;
	unsigned int has_destaddr;
	struct address addr;
};

u32 netStart = 0;
void NetInit()
{
	netStart = 1;
	while(netStart == 1)
		mdelay(100);
}

u32 NetThread(void *arg)
{
	while(!netStart)
		mdelay(100);

	char *soDev = "/dev/net/ip/top";
	void *name = heap_alloc_aligned(0,32,32);
	memcpy(name,soDev,32);
	int soFd = IOS_Open(name,0);
	heap_free(0,name);
	//SOStartup
	IOS_Ioctl(soFd,0x1F,0,0,0,0);
	//SOGetHostId
	int ip = 0;
	do
	{
		mdelay(500);
		ip = IOS_Ioctl(soFd,16,0,0,0,0);
	} while(ip == 0);

	//Basic network up
	netStart = 0;

	//SOSocket
	unsigned int *params = (unsigned int*)heap_alloc_aligned(0,12,32);
	params[0] = AF_INET;
	params[1] = SOCK_STREAM;
	params[2] = IPPROTO_IP;
	int sock = IOS_Ioctl(soFd, 15, params, 12, 0, 0);
	//SOBind
	struct bind_params *bParams = (struct bind_params*)heap_alloc_aligned(0,sizeof(struct bind_params),32);
	memset(bParams,0,sizeof(struct bind_params));
	bParams->socket = sock;
	bParams->has_name = 1;
	bParams->addr.len = 8;
	bParams->addr.family = AF_INET;
	bParams->addr.port = 59944;
	bParams->addr.name = INADDR_ANY;
	IOS_Ioctl(soFd, 2, bParams, sizeof (struct bind_params), 0, 0);
	//SOListen
	params[0] = sock;
	params[1] = 1;
	IOS_Ioctl(soFd, 10, params, 8, 0, 0);
	//Say PPC to continue
	//*(volatile unsigned int*)0x120F0000 = ip;
	//sync_after_write((void*)0x120F0000, 0x20);
	//SOAccept
	memset(bParams,0,sizeof(struct bind_params));
	bParams->addr.len = 8;
	bParams->addr.family = AF_INET;
	params[0] = sock;
	int pcSock = IOS_Ioctl(soFd, 1, params, 4, &bParams->addr, 8);
	heap_free(0,params);
	heap_free(0,bParams);
	//SOSendTo preparation
	struct sendto_params *sParams = (struct sendto_params*)heap_alloc_aligned(0,sizeof(struct sendto_params),32);
	memset(sParams,0,sizeof(struct sendto_params));
	sParams->socket = pcSock;
	sParams->flags = 0;
	sParams->has_destaddr = 0;
	unsigned int *msg = (unsigned int*)heap_alloc_aligned(0,4,32);
	ioctlv *sendVec = (ioctlv*)heap_alloc_aligned(0,sizeof(ioctlv)*2,32);
	sendVec[0].data = msg;
	sendVec[0].len = 4;
	sendVec[1].data = sParams;
	sendVec[1].len = sizeof(struct sendto_params);

	while(1)
	{
		sync_before_read((void*)0x13002800,0x20);
		msg[0] = read32(0x13002800);
		//SOSendTo
		IOS_Ioctlv(soFd, 13, 2, 0, sendVec);
		mdelay(10);
	}
	return 0;
}
