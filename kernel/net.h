#ifndef _NET_H_
#define _NET_H_

#define INADDR_ANY	0
#define IPPROTO_IP	0
#define SOCK_STREAM	1
#define AF_INET		2

/* From marcan - see git://git.bootmii.org/var/git/mini.git
 * Looks like this is also used in libogc code */

#define ALIGNED(x) __attribute__((aligned(x)))
#define STACK_ALIGN(type, name, cnt, alignment)				\
	u8 _al__##name[((sizeof(type)*(cnt)) + (alignment) +		\
	(((sizeof(type)*(cnt))%(alignment)) > 0 ? ((alignment) -	\
	((sizeof(type)*(cnt))%(alignment))) : 0))];			\
	type *name = (type*)(((u32)(_al__##name)) + ((alignment) - ((	\
	(u32)(_al__##name))&((alignment)-1))))


/* Structures describing IOCTL parameters */

struct in_addr {
	u32 s_addr;
};

struct sockaddr_in {
	u8 sin_len;
	u8 sin_family;
	u16 sin_port;
	struct in_addr sin_addr;
	s8 sin_zero[8];
} __attribute__((packed));

struct sockaddr {
	u8 sa_len;
	u8 sa_family;
	u8 sa_data[14];
} __attribute__((packed));

struct address {
	unsigned char len;
	unsigned char family;
	unsigned short port;
	unsigned int name;
	unsigned char unused[20];
};

struct bind_params {
	u32 socket;
	u32 has_name;
	u8 name[28];
	//struct address name;
};

struct connect_params {
	unsigned int socket;
	unsigned int has_addr;
	struct address addr;
};

struct sendto_params {
	unsigned int socket;
	unsigned int flags;
	unsigned int has_destaddr;
	u8 destaddr[28];
	//struct address addr;
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
int NetInit(void);
u32 net_handler(void *arg);

#endif
