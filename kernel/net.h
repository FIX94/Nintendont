#ifndef _NET_H_
#define _NET_H_

#define INADDR_ANY	0
#define IPPROTO_IP	0
#define SOCK_STREAM	1
#define AF_INET		2

/* Structures describing IOCTL parameters */

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


/* These IOCTL definitions are from `libogc/network_wii.c`, and are also 
 * in Dolphin (I think?) */

enum {
	IOCTL_SO_ACCEPT	= 1,
	IOCTL_SO_BIND,
	IOCTL_SO_CLOSE,
	IOCTL_SO_CONNECT,
	IOCTL_SO_FCNTL,
	IOCTL_SO_GETPEERNAME, // todo
	IOCTL_SO_GETSOCKNAME, // todo
	IOCTL_SO_GETSOCKOPT,  // todo    8
	IOCTL_SO_SETSOCKOPT,
	IOCTL_SO_LISTEN,
	IOCTL_SO_POLL,        // todo    b
	IOCTLV_SO_RECVFROM,
	IOCTLV_SO_SENDTO,
	IOCTL_SO_SHUTDOWN,    // todo    e
	IOCTL_SO_SOCKET,
	IOCTL_SO_GETHOSTID,
	IOCTL_SO_GETHOSTBYNAME,
	IOCTL_SO_GETHOSTBYADDR,// todo
	IOCTLV_SO_GETNAMEINFO, // todo   13
	IOCTL_SO_UNK14,        // todo
	IOCTL_SO_INETATON,     // todo
	IOCTL_SO_INETPTON,     // todo
	IOCTL_SO_INETNTOP,     // todo
	IOCTLV_SO_GETADDRINFO, // todo
	IOCTL_SO_SOCKATMARK,   // todo
	IOCTLV_SO_UNK1A,       // todo
	IOCTLV_SO_UNK1B,       // todo
	IOCTLV_SO_GETINTERFACEOPT, // todo
	IOCTLV_SO_SETINTERFACEOPT, // todo
	IOCTL_SO_SETINTERFACE,     // todo
	IOCTL_SO_STARTUP,           // 0x1f
	IOCTL_SO_ICMPSOCKET =	0x30, // todo
	IOCTLV_SO_ICMPPING,         // todo
	IOCTL_SO_ICMPCANCEL,        // todo
	IOCTL_SO_ICMPCLOSE          // todo
};


void net_shutdown(void);
void NetInit(void);
u32 net_handler(void *arg);

#endif
