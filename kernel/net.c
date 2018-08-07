
/* Mostly fixing up FIX94's net.c from the bbatest branch (ha-ha).
 *
 * This prints an endless stream of 0xdeadbeefs to my terminal, wee!
 * You can test this with something like `nc <Wii IP> 666 | xxd`.
 *
 * I think that for now it's reasonable to keep /dev/net/ip/top's file
 * descriptor in some global state in this file so that we don't need to
 * pass it via arguments when implementing the socket operations (so that
 * the interfaces will look just like libogc's.
 *
 * Also need to be weary of the fact that Ioctls expect pointer arguments to
 * be 32-byte aligned.
 *
 */

#include "global.h"
#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"

static u32 net_thread = 0;
extern char __net_stack_addr, __net_stack_size;

static char top_filename[] ALIGNED(32) = "/dev/net/ip/top";

// Let this global hold the file descriptor for /dev/net/ip/top
static s32 ALIGNED(32) top_fd = -1;

/* Returns the current IP address associated with the device.
 * This will block until we recieve an answer from IOS.  */
int current_ip_address(s32 fd)
{
	//SOGetHostId
	int ip = 0;
	do
	{
		mdelay(500);
		ip = IOS_Ioctl(top_fd, 16, 0, 0, 0, 0);
	} while(ip == 0);
	dbgprintf("ip: 0x%x\n", ip);
	return ip;
}

/* Return a file descriptor to a new socket */
s32 socket(u32 domain, u32 type, u32 protocol)
{
	s32 res;

	// Ioctl len_in is 12
	STACK_ALIGN(u32, params, 3, 32);

	if (top_fd < 0) return -62;

	params[0] = domain;
	params[1] = type;
	params[2] = protocol;

	res = IOS_Ioctl(top_fd, IOCTL_SO_SOCKET, params, 12, 0, 0);

	dbgprintf("socket(%d, %d, %d) = %d\n", domain, type, protocol, res);
	return res;
}

s32 close(s32 socket)
{
	s32 res;
	STACK_ALIGN(u32, params, 1, 32);
	if (top_fd < 0) return -62;

	*params = socket;
	res = IOS_Ioctl(top_fd, IOCTL_SO_CLOSE, params, 4, 0, 0);

	dbgprintf("close(%d) = %d\n", socket, res);
	return res;
}

s32 bind(s32 socket, u16 port)//, socklen_t address_len)
{
	s32 res;

	STACK_ALIGN(struct bind_params, params, 1, 32);

	if (top_fd < 0) return -62;

	memset(&params, 0, sizeof(struct bind_params));
	params->socket = socket;
	params->has_name = 1;
	params->addr.len = 8;
	params->addr.name = INADDR_ANY;
	params->addr.port = port;
	params->addr.family = AF_INET;

	res = IOS_Ioctl(top_fd, IOCTL_SO_BIND, params,
			sizeof(struct bind_params), 0, 0);

	dbgprintf("bind(%d, %d, ..) = %d\n", socket, port, res);
	return res;
}

s32 listen(s32 socket, u32 backlog)
{
	s32 res;

	// Ioctl len_in is 8
	STACK_ALIGN(u32, params, 2, 32);

	if (top_fd < 0) return -62;

	params[0] = socket;
	params[1] = backlog;

	res = IOS_Ioctl(top_fd, IOCTL_SO_BIND, &params, 8, 0, 0);

	dbgprintf("listen(%d, %d) = %d\n", socket, backlog, res);
	return res;
}

s32 accept(s32 socket, struct address *addr)
{
	s32 res;

	// Ioctl len_in is 4
	STACK_ALIGN(u32, params, 1, 32);

	if (top_fd < 0) return -62;

	*params = (u32)socket;

	// Might wanna assume this to avoid the extra writes
	addr->len = 8;
	addr->family = AF_INET;

	res = IOS_Ioctl(top_fd, IOCTL_SO_ACCEPT, &params, 4, addr, 8);

	dbgprintf("accept(%d, %p) = %d\n", socket, addr, res);
	return res;
}

s32 sendto(s32 socket, void *data, s32 len, u32 flags)
{
	s32 res;
	STACK_ALIGN(struct sendto_params, params, 1, 32);
	STACK_ALIGN(ioctlv, vec, 2, 32);

	if (top_fd < 0) return -62;

	/* We probably need to get aligned heap for this, which means we need
	 * to memcpy from `void *data` :^/ (unless you just enforce that the
	 * user-provided pointer should always be aligned, lol?)
	 */

	u8 *message_buf = (u8*)heap_alloc_aligned(0, len, 32);
	if (message_buf == NULL) {
		dbgprintf("sendto() couldn't heap_alloc_aligned() - uh oh\n");
		return -1;
	}

	// I don't see how this won't totally kill performance
	memset(&params, 0, sizeof(struct sendto_params));
	memcpy(message_buf, data, len);

	params->socket = socket;
	params->flags = flags;
	params->has_destaddr = 0; // ?

	vec[0].data = message_buf;
	vec[0].len = len;
	vec[1].data = &params;
	vec[1].len = sizeof(struct sendto_params);

	res = IOS_Ioctlv(top_fd, IOCTLV_SO_SENDTO, 2, 0, &vec);

	if (message_buf != NULL)
		heap_free(0, message_buf);

	return res;
}

s32 connect(s32 socket, struct address *addr)
{
	s32 res;
	STACK_ALIGN(struct connect_params, params, 1, 32);

	if (top_fd < 0) return -62;

	// Might wanna assume this to avoid the extra writes
	addr->len = 8;
	addr->family = AF_INET;

	memset(&params, 0, sizeof(struct connect_params));
	params->socket = socket;
	params->has_addr = 1;
	memcpy(&params->addr, addr, 8); // always 8 I think?

	res = IOS_Ioctl(top_fd, IOCTL_SO_CONNECT, &params,
			sizeof(struct connect_params), 0, 0);

	dbgprintf("connect(%d, %p) = %d\n", socket, addr, res);
	return res;
}

u32 net_handler(void *arg)
{
	dbgprintf("net_handler TID: %d\n", thread_get_id());

	/* Get a handle to /dev/net/ip/top and initialize it */

	//int top_fd = IOS_Open(top_filename, 0);

	top_fd = IOS_Open(top_filename, 0);
	IOS_Ioctl(top_fd, IOCTL_SO_STARTUP, 0, 0, 0, 0);

	dbgprintf("top_fd: %d\n", top_fd);
	if (top_fd < 0) return -1;

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

