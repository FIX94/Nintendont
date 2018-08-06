
/* Mostly fixing up FIX94's net.c from the bbatest branch (ha-ha).
 *
 * This prints an endless stream of 0xdeadbeefs to my terminal, wee!
 * You can test this with something like `nc <Wii IP> 666 | xxd`. 
 */

#include "global.h"
#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"

static u32 net_thread = 0;
extern char __net_stack_addr, __net_stack_size;

u32 net_handler(void *arg)
{
	dbgprintf("net_handler TID: %d\n", thread_get_id());

	/* Get a handle to IOS networking device. */

	char *top_fn = "/dev/net/ip/top";
	void *name = heap_alloc_aligned(0, 32, 32);
	memcpy(name, top_fn, 32);
	int top_fd = IOS_Open(name,0);
	dbgprintf("top_fd: %d\n", top_fd);

	heap_free(0,name);

	/* Initialize the top interface? */

	//SOStartup
	IOS_Ioctl(top_fd, IOCTL_SO_STARTUP, 0, 0, 0, 0);

	/* Get our IP address? */

	//SOGetHostId
	int ip = 0;
	do
	{
		mdelay(500);
		ip = IOS_Ioctl(top_fd, 16, 0, 0, 0, 0);
	} while(ip == 0);
	dbgprintf("ip: 0x%x\n", ip);

	/* Get an fd for a new socket */

	//SOSocket
	unsigned int *params = (unsigned int*)heap_alloc_aligned(0, 12, 32);
	params[0] = AF_INET;
	params[1] = SOCK_STREAM;
	params[2] = IPPROTO_IP;

	int sock = IOS_Ioctl(top_fd, IOCTL_SO_SOCKET, params, 12, 0, 0);
	dbgprintf("sock: %d\n", sock);

	/* Bind to socket */

	//SOBind
	struct bind_params *bParams = (struct bind_params*)heap_alloc_aligned(0, sizeof(struct bind_params), 32);

	memset(bParams,0,sizeof(struct bind_params));
	bParams->socket = sock;
	bParams->has_name = 1;
	bParams->addr.len = 8;
	bParams->addr.family = AF_INET;
	bParams->addr.port = 666;
	bParams->addr.name = INADDR_ANY;

	IOS_Ioctl(top_fd, IOCTL_SO_BIND, bParams, 
			sizeof(struct bind_params), 0, 0);

	/* Listen on socket */

	//SOListen
	params[0] = sock;
	params[1] = 1;

	IOS_Ioctl(top_fd, IOCTL_SO_LISTEN, params, 8, 0, 0);


	/* Loop for handling impinging connections */

	while(1) {

		/* Accept a client */

		//SOAccept
		memset(bParams,0,sizeof(struct bind_params));
		bParams->addr.len = 8;
		bParams->addr.family = AF_INET;
		params[0] = sock;

		int pcSock = IOS_Ioctl(top_fd, IOCTL_SO_ACCEPT, params, 4, &bParams->addr, 8);

		heap_free(0,params);
		heap_free(0,bParams);

		/* Prepare to send something */

		//SOSendTo preparation
		struct sendto_params *sParams = (struct sendto_params*)
			heap_alloc_aligned(0, sizeof(struct sendto_params), 32);

		memset(sParams,0,sizeof(struct sendto_params));
		sParams->socket = pcSock;
		sParams->flags = 0;
		sParams->has_destaddr = 0;

		unsigned int *msg = (unsigned int*)heap_alloc_aligned(0, 4, 32);
		ioctlv *sendVec = (ioctlv*)heap_alloc_aligned(0, sizeof(ioctlv) * 2, 32);
		sendVec[0].data = msg;
		sendVec[0].len = 4;
		sendVec[1].data = sParams;
		sendVec[1].len = sizeof(struct sendto_params);

		/* Hit the client with a constant stream of 0xdeadbeef */

		while (1) 
		{
			msg[0] = 0xdeadbeef;
			
			//SOSendTo
			IOS_Ioctlv(top_fd, IOCTLV_SO_SENDTO, 2, 0, sendVec);
			mdelay(10);
		}
	}
	return 0;
}

void net_shutdown(void) { thread_cancel(net_thread, 0); }
void NetInit(void)
{
	dbgprintf("net_thread is starting ...\n");
	net_thread = do_thread_create(net_handler, ((u32*)&__net_stack_addr), 
			((u32)(&__net_stack_size)), 0x70);
	thread_continue(net_thread);
}

