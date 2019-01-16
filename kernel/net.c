/* Functions for network initialization and socket operations -- based on
 * FIX94's experimental bbatest branch in upstream Nintendont, and also on 
 * the libogc articulation of networking with IOS (network_wii.c).
 *
 * NOTES FOR DEVELOPERS
 * ====================
 * Recall that, when issuing ioctls to IOS, all pointer arguments need to be
 * 32-byte aligned (or at least, this is what the doctor tells me). This is
 * also the reasoning behind the STACK_ALIGN macro usage.
 *
 * Aspects of low-level network state should probably be managed in this file 
 * and exposed as globals - for instance, after network initialization, the 
 * file descriptor associated with /dev/net/ip/top can simply be used by other 
 * threads by extern-ing `s32 top_fd` [for now].
 */

#include "global.h"
#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"

#include "SlippiDebug.h"

// These are only relevant to NCDInit(), and are closed before returning
static s32 ncd_fd __attribute__((aligned(32)));
static s32 kd_fd  __attribute__((aligned(32)));

// This is global state, used elsewhere when dealing with sockets [for now]
s32 top_fd __attribute__((aligned(32)));

// Used by kernel/main.c
u32 NetworkStarted; 


/* NCDInit() handles IOS network initialization, and needs to complete before 
 * any other attempt to deal with sockets. It should ultimately prepare top_fd
 * for use in dedicated Slippi thread/s. 
 */

int NCDInit(void)
{
	s32 res;
	STACK_ALIGN(ioctlv, vec, 1, 32);

	dbgprintf("entered NCDInit()\n");

	char *ncd_filename = "/dev/net/ncd/manage";
	char *top_filename = "/dev/net/ip/top";
	char *kd_filename  = "/dev/net/kd/request";

	void *ncd_name = heap_alloc_aligned(0, 32, 32);
	void *top_name = heap_alloc_aligned(0, 32, 32);
	void *kd_name  = heap_alloc_aligned(0, 32, 32);

	memcpy(ncd_name, ncd_filename, 32);
	memcpy(top_name, top_filename, 32);
	memcpy(kd_name,  kd_filename, 32);

	/* Emit NCD_GETLINKSTATUS on /dev/net/ncd/manage.
	 * Seems like we just take a single 32-byte buffer?
	 */

	ncd_fd = IOS_Open(ncd_name, 0);
	dbgprintf("ncd_fd: %i\n", ncd_fd);

	void *message_buf = heap_alloc_aligned(0,32,32);
	vec[0].data = message_buf;
	vec[0].len = 32;

	res = IOS_Ioctlv(ncd_fd, IOCTL_NCD_GETLINKSTATUS, 0, 1, vec);
	dbgprintf("IOCTL_NCD_GETLINKSTATUS: %d\n", res);

	IOS_Close(ncd_fd);

	/* Emit NWC32_STARTUP on /dev/net/kd/request.
	 * According to libogc, some error code should fall out in the
	 * buffer which indicates some success or failure.
	 */

	kd_fd = IOS_Open(kd_name, 0);
	dbgprintf("kd_fd: %i\n", kd_fd);

	void *nwc_buf = heap_alloc_aligned(0,32,32);
	res = IOS_Ioctl(kd_fd, IOCTL_NWC24_STARTUP, 0, 0, nwc_buf, 32);
	dbgprintf("IOCTL_NWC24_STARTUP: %d\n", res);

	// libogc does `memcpy(&some_s32, nwc_buf, sizeof(some_s32))` here.
	// if (some_s32 == -29), retry?
	// if (some_s32 == -15), already started?

	IOS_Close(kd_fd);


	/* Emit IOCTL_SO_STARTUP on /dev/net/ip/top, and then emit
	 * IOCTL_SO_GETHOSTID to retrieve some representation of our IP?
	 */

	top_fd = IOS_Open(top_name, 0);
	dbgprintf("top_fd: %i\n", top_fd);

	res = IOS_Ioctl(top_fd, IOCTL_SO_STARTUP, 0, 0, 0, 0);
	dbgprintf("IOCTL_SO_STARTUP: %d\n", res);

	// Ideally we sit in a loop here until we can validate our IP?
	res = IOS_Ioctl(top_fd, IOCTL_SO_GETHOSTID, 0, 0, 0, 0);
	dbgprintf("IOCTL_SO_GETHOSTID: 0x%x\n", res);

	heap_free(0, top_name);
	heap_free(0, ncd_name);
	heap_free(0, kd_name);
	heap_free(0, nwc_buf);

	NetworkStarted = 1;
	ppc_msg("NCDINIT OK\x00", 11);
	return 0;
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

	return res;
}

/* Close a socket */
s32 close(s32 fd, s32 socket)
{
	s32 res;
	STACK_ALIGN(u32, params, 1, 32);
	if (fd < 0) return -62;

	*params = socket;
	res = IOS_Ioctl(fd, IOCTL_SO_CLOSE, params, 4, 0, 0);

	return res;
}

/* Bind a name to some socket */
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

/* Listen for connections on a socket */
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

/* Accept some connection on a socket */
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

/* Emit a message on a socket */
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

/* Recieve a message from some socket */
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

/* Connect to some remote server */
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

/* Set options on some socket. Implementation not tested yet. */
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
			sizeof(struct setsockopt_params), 0, 0);

	return res;
}

/* Implementation not tested yet. */
static union ullc { u64 ull; u32 ul[2]; };
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
