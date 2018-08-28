
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

//static char *top_filename ALIGNED(32) = "/dev/net/ip/top";

/* Returns the current IP address associated with the device.
 * This will block until we recieve an answer from IOS.  */
int current_ip_address(s32 fd)
{
	//SOGetHostId
	int ip = 0;
	do
	{
		mdelay(1000);
		ip = IOS_Ioctl(fd, 16, 0, 0, 0, 0);
	} while(ip <= 0);
	dbgprintf("ip: 0x%x\r\n", ip);
	return ip;
}

/* Return a file descriptor to a new socket */
s32 socket(s32 fd, u32 domain, u32 type, u32 protocol)
{
	s32 res;

	// Ioctl len_in is 12
	STACK_ALIGN(u32, params, 3, 32);

	if (fd < 0) return -62;

	params[0] = domain;
	params[1] = type;
	params[2] = protocol;

	res = IOS_Ioctl(fd, IOCTL_SO_SOCKET, params, 12, 0, 0);

	dbgprintf("socket(%d, %d, %d, %d) = %d\r\n", 
			fd, domain, type, protocol, res);
	return res;
}

s32 close(s32 fd, s32 socket)
{
	s32 res;
	STACK_ALIGN(u32, params, 1, 32);
	if (fd < 0) return -62;

	*params = socket;
	res = IOS_Ioctl(fd, IOCTL_SO_CLOSE, params, 4, 0, 0);

	dbgprintf("close(%d, %d) = %d\r\n", fd, socket, res);
	return res;
}

s32 bind(s32 fd, s32 socket, struct sockaddr *name)//, socklen_t address_len)
{
	s32 res;

	STACK_ALIGN(struct bind_params, params, 1, 32);

	if (fd < 0) return -62;

	name->sa_len = 8;

	memset(params, 0, sizeof(struct bind_params));
	params->socket = socket;
	params->has_name = 1;
	memcpy(params->name, name, 8);

	res = IOS_Ioctl(fd, IOCTL_SO_BIND, params,
			sizeof(struct bind_params), 0, 0);

	dbgprintf("bind(%d, %d, %p) = %d\r\n", fd, socket, name);
	return res;
}

s32 listen(s32 fd, s32 socket, u32 backlog)
{
	s32 res;

	// Ioctl len_in is 8
	STACK_ALIGN(u32, params, 2, 32);

	if (fd < 0) return -62;

	params[0] = socket;
	params[1] = backlog;

	res = IOS_Ioctl(fd, IOCTL_SO_LISTEN, params, 8, 0, 0);

	dbgprintf("listen(%d, %d, %d) = %d\r\n", fd, socket, backlog, res);
	return res;
}

// I think ioctl writes into addr - might be a problem if it isn't aligned?
s32 accept(s32 fd, s32 socket)
{
	s32 res;

	// Ioctl len_in is 4
	STACK_ALIGN(u32, params, 1, 32);
	STACK_ALIGN(struct sockaddr, addr, 1, 32);

	if (fd < 0) return -62;

	*params = socket;

	addr->sa_len = 8;
	addr->sa_family = AF_INET;

	res = IOS_Ioctl(fd, IOCTL_SO_ACCEPT, params, 4, addr, 8);
	// dbgprintf("accept(%d, %d) = %d\r\n", fd, socket, res);

	return res;
}

s32 sendto(s32 fd, s32 socket, void *data, s32 len, u32 flags)
{
	s32 res;
	STACK_ALIGN(struct sendto_params, params, 1, 32);
	STACK_ALIGN(ioctlv, vec, 2, 32);

	if (fd < 0) return -62;

	/* We probably need to get aligned heap for this, which means we need
	 * to memcpy from `void *data` :^/ (unless you just enforce that the
	 * user-provided pointer should always be aligned, lol?)
	 */

	u8 *message_buf = (u8*)heap_alloc_aligned(0, len, 32);
	if (message_buf == NULL) {
		dbgprintf("sendto() couldn't heap_alloc_aligned() - uh oh\r\n");
		return -1;
	}

	// I don't see how this won't totally kill performance
	memset(params, 0, sizeof(struct sendto_params));
	memcpy(message_buf, data, len);

	params->socket = socket;
	params->flags = flags;
	params->has_destaddr = 0; // ?
	//memcpy(params->destaddr, to, to->sa_len);

	vec[0].data = message_buf;
	vec[0].len = len;
	vec[1].data = params;
	vec[1].len = sizeof(struct sendto_params);

	res = IOS_Ioctlv(fd, IOCTLV_SO_SENDTO, 2, 0, vec);

	if (message_buf != NULL)
		heap_free(0, message_buf);

	dbgprintf("sendto(%d, %d, %p, %d, %d) = %d\r\n",
			fd, socket, data, len, flags);

	return res;
}


/* For now, just assume that we provide an aligned address and
 * disregard having to allocate heap to do memcpy things */
s32 recvfrom(s32 fd, s32 socket, void *mem, s32 len, u32 flags)
{
	s32 res;
	u8* message_buf = NULL;
	STACK_ALIGN(u32, params, 2, 32);
	STACK_ALIGN(ioctlv, vec, 3, 32);

	if (fd < 0) return -62;
	if (len <= 0) return -28;

	params[0] = socket;
	params[1] = flags;

	vec[0].data = params;
	vec[0].len = 8;
	vec[1].data = mem;
	vec[1].len = len;
	vec[2].data = NULL;
	vec[2].len = 0;

	res = IOS_Ioctlv(fd, IOCTLV_SO_RECVFROM, 2, 0, vec);

	return res;
}




s32 connect(s32 fd, s32 socket, struct address *addr)
{
	s32 res;
	STACK_ALIGN(struct connect_params, params, 1, 32);

	if (fd < 0) return -62;

	// Might wanna assume this to avoid the extra writes
	addr->len = 8;
	addr->family = AF_INET;

	memset(&params, 0, sizeof(struct connect_params));
	params->socket = socket;
	params->has_addr = 1;
	memcpy(&params->addr, addr, 8); // always 8 I think?

	res = IOS_Ioctl(fd, IOCTL_SO_CONNECT, &params,
			sizeof(struct connect_params), 0, 0);

	dbgprintf("connect(%d, %p) = %d\r\n", socket, addr, res);
	return res;
}


static struct sockaddr_in server __attribute__((aligned(32)));
static int sock __attribute__((aligned(32)));

//static int client_sock __attribute__((aligned(32)));
//static s32 top_fd __attribute__((aligned(32)));
int client_sock __attribute__((aligned(32)));
s32 top_fd __attribute__((aligned(32)));

static u8 message[0x100] __attribute__((aligned(32)));


u32 net_handler(void *arg)
{
	s32 res;
	dbgprintf("net_handler TID: %d\r\n", thread_get_id());

	// Sleep 10 seconds. hopefully this helps the game start correctly
	mdelay(20000);

	while (1)
	{
		// Wait for a client to connect
		mdelay(10);
		client_sock = accept(top_fd, sock);

		if (client_sock >= 0) {
			while (1) 
			{

			// We expect the Slippi thread to send messages
			// on client_sock here. Just wait until the
			// socket is closed by the Slippi thread.

				mdelay(100);
				if (client_sock < 0) break;
			}
		}

	}
	return 0;
}


void net_shutdown(void) { thread_cancel(net_thread, 0); }
int NetInit(void)
{
	s32 res;
	dbgprintf("setting up state for net_thread ...\r\n");

	char *top_filename = "/dev/net/ip/top";
	void *name = heap_alloc_aligned(0,32,32);
	memcpy(name, top_filename, 32);
	top_fd = IOS_Open(name,0);
	heap_free(0,name);
	IOS_Ioctl(top_fd, IOCTL_SO_STARTUP, 0, 0, 0, 0);
	dbgprintf("top_fd: %d\r\n", top_fd);
	if (top_fd < 0) return -1;

	sock = socket(top_fd, AF_INET, SOCK_STREAM, IPPROTO_IP);
	dbgprintf("sock: %d\r\n", sock);

	/* Bind to socket and start listening */

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = 666;
	server.sin_addr.s_addr = INADDR_ANY;

	res = bind(top_fd, sock, (struct sockaddr*)&server);
	if (res < 0) {
		close(top_fd, sock);
		return res;
	}

	res = listen(top_fd, sock, 1);
	if (res < 0)  {
		close(top_fd, sock);
		return res;
	}

	dbgprintf("net_thread is starting ...\r\n");
	net_thread = do_thread_create(net_handler, ((u32*)&__net_stack_addr),
			((u32)(&__net_stack_size)), 0x78);
	thread_continue(net_thread);

	return 0;
}

