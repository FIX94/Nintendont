/* kernel/net.c
 * Functions for network initialization and socket operations -- based on
 * FIX94's experimental bbatest branch in upstream Nintendont, and also on
 * the libogc articulation of networking with IOS (network_wii.c).
 */

#include "global.h"
#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"

#include "SlippiDebug.h"

// File descriptor for the IOS socket driver, shared by all Slippi threads
s32 top_fd __attribute__((aligned(32)));

// Global state - Wi-Fi MAC address and current IP address
u8 wifi_mac_address[6] ALIGNED(32) = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
u32 current_ip_address ALIGNED(32) = 0x00000000;

// Used by kernel/main.c
u32 NetworkStarted;

// Device filenames bound to different IOS drivers
static const char  kd_name[19] ALIGNED(32) = "/dev/net/kd/request";
static const char ncd_name[19] ALIGNED(32) = "/dev/net/ncd/manage";
static const char top_name[15] ALIGNED(32) = "/dev/net/ip/top";


/* NCDInit()
 * Handles IOS network initialization, and needs to complete before any other
 * attempt to deal with sockets. It should ultimately prepare top_fd for use
 * in dedicated Slippi networking thread/s.
 */
int NCDInit(void)
{
	s32 res;
	STACK_ALIGN(ioctlv, mac_vec, 2, 32);
	dbgprintf("entered NCDInit()\n");

	/* Fetch the MAC address of the Wi-Fi interface for use later when we
	 * broadcast messages to potential Slippi clients.
	 */

	s32 ncd_fd = IOS_Open(ncd_name, 0);
	void *ncd_buf = heap_alloc_aligned(0, 32, 32);
	dbgprintf("ncd_fd: %i\n", ncd_fd);

	mac_vec[0].data	= ncd_buf;
	mac_vec[0].len	= 32;
	mac_vec[1].data	= &wifi_mac_address;
	mac_vec[1].len	= 6;

	res = IOS_Ioctlv(ncd_fd, IOCTLV_NCD_GETMACADDRESS, 0, 2, mac_vec);
	IOS_Close(ncd_fd);
	heap_free(0, ncd_buf);


	/* NWC24 initialization. This happens in libogc, but do we actually
	 * *need* to do this? Is NWC24 state somehow tied to whether or not
	 * we can use the socket driver?
	 *
	 * NOTE: libogc checks the return value of this in order to determine
	 * if NWC24 has started up correctly:
	 *
	 *	memcpy(&some_s32, nwc_buf, sizeof(some_s32));
	 *	if (some_s32 == -29) retry;
	 *	if (some_s32 == -15) already_started;
	 */

	s32 kd_fd = IOS_Open(kd_name, 0);
	void *nwc_buf = heap_alloc_aligned(0, 32, 32);
	dbgprintf("kd_fd: %i\n", kd_fd);

	res = IOS_Ioctl(kd_fd, IOCTL_NWC24_STARTUP, 0, 0, nwc_buf, 32);
	dbgprintf("IOCTL_NWC24_STARTUP: %d\n", res);
	IOS_Close(kd_fd);
	heap_free(0, nwc_buf);


	/* Get a file descriptor for the IOS socket driver [to share with all
	 * Slippi threads], then initialize the IOS socket driver.
	 */

	top_fd = IOS_Open(top_name, 0);
	dbgprintf("top_fd: %i\n", top_fd);

	res = IOS_Ioctl(top_fd, IOCTL_SO_STARTUP, 0, 0, 0, 0);
	dbgprintf("IOCTL_SO_STARTUP: %d\n", res);

	current_ip_address = IOS_Ioctl(top_fd, IOCTL_SO_GETHOSTID, 0, 0, 0, 0);
	dbgprintf("IOCTL_SO_GETHOSTID: 0x%x\n", current_ip_address);

	NetworkStarted = 1;
	ppc_msg("NCDINIT OK\x00", 11);
	return 0;
}


/* ----------------------------------------------------------------------------
 * Socket API functions
 *
 * These are wrappers around ioctl() calls used to ask different things of the
 * IOS socket driver. The implementation here is pretty much the same as in
 * libogc.
 *
 * All of these functions expect a file descriptor to /dev/net/ip/top as the
 * first argument (in case sometime later we end up having to juggle many FDs).
 */


/* socket()
 * Returns a file descriptor for a new socket.
 */
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

	return res;
}

/* close()
 * Closes a socket.
 */
s32 close(s32 fd, s32 socket)
{
	s32 res;
	STACK_ALIGN(u32, params, 1, 32);
	if (fd < 0) return -62;

	*params = socket;
	res = IOS_Ioctl(fd, IOCTL_SO_CLOSE, params, 4, 0, 0);

	return res;
}

/* bind()
 * Bind some name to some socket.
 */
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

	return res;
}

/* listen()
 * Start listening for connections on a socket.
 */
s32 listen(s32 fd, s32 socket, u32 backlog)
{
	s32 res;

	// Ioctl len_in is 8
	STACK_ALIGN(u32, params, 2, 32);

	if (fd < 0) return -62;

	params[0] = socket;
	params[1] = backlog;

	res = IOS_Ioctl(fd, IOCTL_SO_LISTEN, params, 8, 0, 0);

	return res;
}

/* accept()
 * Accept a connection on some socket.
 * Returns a file descriptor representing the remote client.
 */
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

	return res;
}

/* sendto()
 * Emit a message on some socket.
 */
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

	return res;
}

/* recvfrom()
 * Recieve a message on some socket.
 */
s32 recvfrom(s32 fd, s32 socket, void *mem, s32 len, u32 flags)
{
	s32 res;
	//u8* message_buf = NULL;
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

/* connect()
 * Connect to some remote server.
 */
s32 connect(s32 fd, s32 socket, struct sockaddr *name)
{
	s32 res;
	STACK_ALIGN(struct connect_params, params, 1, 32);

	if (fd < 0) return -62;

	name->sa_len = 8;

	memset(params, 0, sizeof(struct connect_params));
	params->socket = socket;
	params->has_addr = 1;
	memcpy(params->name, name, 8); // always 8 I think?

	res = IOS_Ioctl(fd, IOCTL_SO_CONNECT, params,
			sizeof(struct connect_params), 0, 0);

	return res;
}

/* setsockopt()
 * Set socket-specific options.
 */
s32 setsockopt(s32 fd, s32 socket, u32 level, u32 optname, void *optval, u32 optlen)
{
	s32 res;
	STACK_ALIGN(struct setsockopt_params, params, 1, 32);

	if (fd < 0) return -62;

	memset(params, 0, sizeof(struct setsockopt_params));
	params->socket = socket;
	params->level = level;
	params->optname = optname;
	params->optlen = optlen;
	if (optval && optlen)
		memcpy(params->optval, optval, optlen);
	res = IOS_Ioctl(fd, IOCTL_SO_SETSOCKOPT, params,
			sizeof(struct setsockopt_params), NULL, 0);

	return res;
}

/* Implementation not tested yet. */
s32 poll(s32 fd, struct pollsd *sds, s32 nsds, s32 timeout)
{
	STACK_ALIGN(u64, params, 1, 32);
	union ullc outv;
	s32 res;

	if (fd < 0) return -62;
	if (sds <= 0) return -28;
	if (nsds <= 0) return -28;

	struct pollsd *psds = (struct pollsd*)heap_alloc_aligned(0,
			(nsds * sizeof(struct pollsd)), 32);

	outv.ul[0] = 0;
	outv.ul[1] = timeout;
	params[0] = outv.ull;

	memcpy(psds, sds, (nsds*sizeof(struct pollsd)));
	res = IOS_Ioctl(fd, IOCTL_SO_POLL, params, 8, psds, (nsds * sizeof(struct pollsd)));
	memcpy(sds, psds, (nsds*sizeof(struct pollsd)));

	heap_free(0, psds);
	return res;
}
