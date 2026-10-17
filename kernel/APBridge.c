/*
 * Nintendont AP Bridge
 *
 * v0.5: UDP HELLO and bounded GameCube MEM1 READ/WRITE/WRTX access.
 */

#include "global.h"
#include "common.h"
#include "Config.h"
#include "sock.h"
#include "string.h"
#include "APBridge.h"
#include "APBridgeProtocol.h"

#define APBRIDGE_STACK_SIZE 0x800
#define APBRIDGE_THREAD_PRIORITY 0x78
#define APBRIDGE_RETRY_MS 5000
#define APBRIDGE_NWC_ATTEMPTS 32
#define APBRIDGE_IP_RETRIES 100
#define APBRIDGE_NCD_OPEN_RETRIES 32
#define APBRIDGE_NCD_RETRY_MS 100
#define APBRIDGE_NCD_GETLINKSTATUS 0x07

/* IOS IPv4/socket values (also used by the installed libOGC network.h). */
#define APBRIDGE_AF_INET 2
#define APBRIDGE_SOCK_DGRAM 2
#define APBRIDGE_SOCKADDR_LEN 8
#define APBRIDGE_NWC_ALREADY_STARTED (-15)
#define APBRIDGE_NWC_STARTUP_PENDING (-29)

/* Exact IOS request layouts from sock.c; not PPC request-table entries. */
struct APBridge_BindParams {
	u32 socket;
	u32 has_name;
	u8 name[28];
};

struct APBridge_SendParams {
	u32 socket;
	u32 flags;
	u32 has_destaddr;
	u8 destaddr[28];
};

static const char APBridge_NCDDevice[] ALIGNED(32) = "/dev/net/ncd/manage";
static const char APBridge_IPDevice[] ALIGNED(32) = "/dev/net/ip/top";

/* Only the bridge thread uses these buffers, one synchronous IOS call at a time.
 * Like usb.c/SDI.c, these are ARM buffers: no PPC cache/interrupt handoff.
 * Align the instances, not the struct types (IOS expects 36/40 byte requests).
 */
static u32 APBridge_Params[8] ALIGNED(32);
static u32 APBridge_Result[8] ALIGNED(32);
static u32 APBridge_ResultLen[8] ALIGNED(32);
static ioctlv APBridge_Vec[4] ALIGNED(32);
static struct APBridge_BindParams APBridge_Bind ALIGNED(32);
static struct APBridge_SendParams APBridge_Send ALIGNED(32);
static u8 APBridge_Peer[32] ALIGNED(32);
/* The extra byte detects datagrams larger than the maximum valid WRITE. */
static u8 APBridge_Packet[APBRIDGE_PACKET_BUFFER_SIZE] ALIGNED(32);
static u8 APBridge_Reply[32] ALIGNED(32) = "NIN-AP";
static u8 APBridge_ReadReply[APBRIDGE_READ_REPLY_SIZE] ALIGNED(32);
/* ARM-only state, shared by all UDP peers and retained across socket retries. */
static struct APBridgeTxCache APBridge_TxCache;

static s32 APBridge_ThreadID = -1;
static u32 *APBridge_Stack = NULL;

static void APBridge_Record(enum APBridgeDiagStage stage, s32 detail)
{
	/* The loader invalidates this same 0x20-byte status line before reading. */
	*(vs32*)(0x10004100 + APBRIDGE_DIAG_DETAIL_OFFSET) = detail;
	*(vu32*)(0x10004100 + APBRIDGE_DIAG_STAGE_OFFSET) = stage;
	sync_after_write(STATUS, 0x20);
}

static bool APBridge_CheckLinkStatus(void)
{
	s32 fd;
	s32 ret;
	u32 attempt;

	fd = -1;

	/*
	 * Match libOGC's network startup sequence:
	 * query NCD link status before opening /dev/net/ip/top.
	 *
	 * /dev/net/ncd/manage may temporarily return IPC_ENOENT
	 * while the network subsystem is becoming available.
	 */
	for(attempt = 0; attempt < APBRIDGE_NCD_OPEN_RETRIES; attempt++)
	{
		APBridge_Record(APBRIDGE_DIAG_NCD_OPEN_BEGIN, 0);
		fd = IOS_Open(APBridge_NCDDevice, 0);

		if(fd >= 0)
			break;

		APBridge_Record(APBRIDGE_DIAG_NCD_OPEN_FAILED, fd);
		if(fd != IPC_ENOENT)
			return false;

		mdelay(APBRIDGE_NCD_RETRY_MS);
	}

	if(fd < 0)
		return false;
	APBridge_Record(APBRIDGE_DIAG_NCD_OPEN_OK, fd);

	memset(APBridge_Result, 0, sizeof(APBridge_Result));

	APBridge_Vec[0].data = APBridge_Result;
	APBridge_Vec[0].len = 0x20;

	APBridge_Record(APBRIDGE_DIAG_NCD_GETLINKSTATUS_BEGIN, 0);
	ret = IOS_Ioctlv(
		fd,
		APBRIDGE_NCD_GETLINKSTATUS,
		0,
		1,
		APBridge_Vec
	);

	IOS_Close(fd);
	APBridge_Record(ret < 0 ? APBRIDGE_DIAG_NCD_GETLINKSTATUS_FAILED
		: APBRIDGE_DIAG_NCD_GETLINKSTATUS_OK, ret);

	return ret >= 0;
}

static bool APBridge_HasIP(s32 fd)
{
	s32 ret;

	/* SOGetInterfaceOpt/IPGetAddr: default interface, IPv4/netmask/broadcast. */
	APBridge_Params[0] = 0xFFFE;
	APBridge_Params[1] = 0x4003;
	memset(APBridge_Result, 0, sizeof(APBridge_Result));
	APBridge_ResultLen[0] = 12;
	APBridge_Vec[0].data = APBridge_Params;
	APBridge_Vec[0].len = 8;
	APBridge_Vec[1].data = APBridge_Result;
	APBridge_Vec[1].len = 12;
	APBridge_Vec[2].data = APBridge_ResultLen;
	APBridge_Vec[2].len = 4;
	ret = IOS_Ioctlv(fd, so_getinterfaceopt, 1, 2, APBridge_Vec);
	if(ret < 0)
	{
		APBridge_Record(APBRIDGE_DIAG_IP_QUERY_FAILED, ret);
		return false;
	}
	if(APBridge_Result[0] == 0)
	{
		APBridge_Record(APBRIDGE_DIAG_IP_NOT_READY, 0);
		return false;
	}
	APBridge_Record(APBRIDGE_DIAG_IP_READY, (s32)APBridge_Result[0]);
	return true;
}

static s32 APBridge_StartNetwork(void)
{
	s32 fd, kd, ret;
	u32 attempt;

	if(!APBridge_CheckLinkStatus())
		return -1;

	APBridge_Record(APBRIDGE_DIAG_IP_TOP_OPEN_BEGIN, 0);
	fd = IOS_Open(APBridge_IPDevice, 0);
	if(fd < 0)
	{
		APBridge_Record(APBRIDGE_DIAG_IP_TOP_OPEN_FAILED, fd);
		return -1;
	}
	APBridge_Record(APBRIDGE_DIAG_IP_TOP_OPEN_OK, fd);

	/* SOCKInit already attempted KD open; only sock.c owns a valid handle. */
	kd = SOCKGetNWCHandle();
	if(kd < 0)
	{
		APBridge_Record(APBRIDGE_DIAG_KD_EXISTING_FD_INVALID, kd);
		goto failed;
	}
	APBridge_Record(APBRIDGE_DIAG_KD_BORROWED_FD, kd);

	/* SOStartup's NWC command starts the saved default connection. SOCKInit
	 * only opens devices. This does not resume the WC24 scheduler suspended
	 * by the loader, or change the user's NCD profile configuration.
	 */
	/* Match libOGC: at most 32 startup attempts, with 100 ms between
	 * temporary (-29) results. Keep the same KD handle throughout.
	 */
	for(attempt = 0; attempt < APBRIDGE_NWC_ATTEMPTS; attempt++)
	{
		APBridge_Record(APBRIDGE_DIAG_NWC_STARTUP_BEGIN, 0);
		memset(APBridge_Result, 0, sizeof(APBridge_Result));
		APBridge_Result[0] = (u32)-1;
		ret = IOS_Ioctl(kd, nwc_startup & ~SO_NWC_CMD, NULL, 0,
			APBridge_Result, 0x20);
		if(ret < 0)
		{
			APBridge_Record(APBRIDGE_DIAG_NWC_STARTUP_FAILED, ret);
			goto failed;
		}
		ret = (s32)APBridge_Result[0];
		if(ret == APBRIDGE_NWC_STARTUP_PENDING)
			APBridge_Record(APBRIDGE_DIAG_NWC_STARTUP_PENDING, ret);
		if(ret != APBRIDGE_NWC_STARTUP_PENDING || attempt + 1 == APBRIDGE_NWC_ATTEMPTS)
			break;
		mdelay(100);
	}
	/* libOGC's IOS startup also accepts -15 (already started), e.g. when
	 * retrying after an address timeout. Other failures back off below.
	 */
	if(ret < 0 && ret != APBRIDGE_NWC_ALREADY_STARTED)
	{
		APBridge_Record(APBRIDGE_DIAG_NWC_STARTUP_FAILED, ret);
		goto failed;
	}
	APBridge_Record(ret == APBRIDGE_NWC_ALREADY_STARTED
		? APBRIDGE_DIAG_NWC_ALREADY_STARTED : APBRIDGE_DIAG_NWC_STARTUP_OK, ret);

	APBridge_Record(APBRIDGE_DIAG_SO_STARTINTERFACE_BEGIN, 0);
	ret = IOS_Ioctl(fd, so_startinterface, NULL, 0, NULL, 0);
	if(ret < 0)
	{
		APBridge_Record(APBRIDGE_DIAG_SO_STARTINTERFACE_FAILED, ret);
		goto failed;
	}
	APBridge_Record(APBRIDGE_DIAG_SO_STARTINTERFACE_OK, ret);

	APBridge_Record(APBRIDGE_DIAG_IP_WAIT_BEGIN, 0);
	for(attempt = 0; attempt < APBRIDGE_IP_RETRIES; attempt++)
	{
		if(APBridge_HasIP(fd))
			return fd;
		mdelay(100);
	}

failed:
	/* Never issue nwc_cleanup: it affects the shared network stack. */
	IOS_Close(fd);
	return -1;
}

static s32 APBridge_OpenSocket(s32 fd)
{
	s32 socket, bindRet;

	APBridge_Params[0] = APBRIDGE_AF_INET;
	APBridge_Params[1] = APBRIDGE_SOCK_DGRAM;
	APBridge_Params[2] = 0; /* default UDP protocol */
	APBridge_Record(APBRIDGE_DIAG_UDP_SOCKET_BEGIN, 0);
	socket = IOS_Ioctl(fd, so_socket, APBridge_Params, 12, NULL, 0);
	if(socket < 0)
	{
		APBridge_Record(APBRIDGE_DIAG_UDP_SOCKET_FAILED, socket);
		return -1;
	}
	APBridge_Record(APBRIDGE_DIAG_UDP_SOCKET_OK, socket);

	memset(&APBridge_Bind, 0, sizeof(APBridge_Bind));
	APBridge_Bind.socket = socket;
	APBridge_Bind.has_name = 1;
	APBridge_Bind.name[0] = APBRIDGE_SOCKADDR_LEN;
	APBridge_Bind.name[1] = APBRIDGE_AF_INET;
	/* Network byte order; remaining address bytes are INADDR_ANY. */
	APBridge_Bind.name[2] = (APBRIDGE_UDP_PORT >> 8) & 0xFF;
	APBridge_Bind.name[3] = APBRIDGE_UDP_PORT & 0xFF;
	APBridge_Record(APBRIDGE_DIAG_UDP_BIND_BEGIN, 0);
	bindRet = IOS_Ioctl(fd, so_bind, &APBridge_Bind, sizeof(APBridge_Bind), NULL, 0);
	if(bindRet < 0)
	{
		APBridge_Record(APBRIDGE_DIAG_UDP_BIND_FAILED, bindRet);
		APBridge_Params[0] = socket;
		IOS_Ioctl(fd, so_close, APBridge_Params, 4, NULL, 0);
		return -1;
	}
	APBridge_Record(APBRIDGE_DIAG_UDP_BIND_OK, bindRet);
	return socket;
}

static void APBridge_Serve(s32 fd, s32 socket)
{
	s32 received;
	u32 replyLength;
	u32 armAddress;
	enum APBridgePacketKind kind;
	enum APBridgeTxLookupResult txResult;
	struct APBridgeMemoryRequest request;
	APBridge_Record(APBRIDGE_DIAG_UDP_SERVING, 0);

	while(1)
	{
		APBridge_Params[0] = socket;
		APBridge_Params[1] = 0; /* blocking receive sleeps only this thread */
		memset(APBridge_Peer, 0, sizeof(APBridge_Peer));
		APBridge_Peer[0] = APBRIDGE_SOCKADDR_LEN;
		APBridge_Peer[1] = APBRIDGE_AF_INET;
		APBridge_Vec[0].data = APBridge_Params;
		APBridge_Vec[0].len = 8;
		APBridge_Vec[1].data = APBridge_Packet;
		APBridge_Vec[1].len = sizeof(APBridge_Packet);
		APBridge_Vec[2].data = APBridge_Peer;
		APBridge_Vec[2].len = APBRIDGE_SOCKADDR_LEN;
		received = IOS_Ioctlv(fd, so_recvfrom, 1, 2, APBridge_Vec);
		if(received < 0)
			return;

		if(APBridge_Peer[0] == APBRIDGE_SOCKADDR_LEN
			&& APBridge_Peer[1] == APBRIDGE_AF_INET)
		{
			kind = APBridge_DecodePacket(APBridge_Packet, (u32)received, &request);
			if(kind != APBRIDGE_PACKET_IGNORE)
			{
				memset(&APBridge_Send, 0, sizeof(APBridge_Send));
				APBridge_Send.socket = socket;
				APBridge_Send.has_destaddr = 1;
				memcpy(APBridge_Send.destaddr, APBridge_Peer, APBRIDGE_SOCKADDR_LEN);
				if(kind == APBRIDGE_PACKET_HELLO)
				{
					APBridge_Vec[0].data = APBridge_Reply;
					replyLength = 6; /* no trailing NUL or newline */
				}
				else
				{
					replyLength = kind == APBRIDGE_PACKET_WRITE_TX
						? APBRIDGE_TX_REQUEST_SIZE : APBRIDGE_READ_REQUEST_SIZE;
					if(request.error == 0)
					{
						if(kind == APBRIDGE_PACKET_READ)
						{
							armAddress = P2C(request.address);
							sync_before_read_align32((void*)armAddress, request.length);
							memcpy(APBridge_ReadReply + APBRIDGE_READ_REQUEST_SIZE,
								(const void*)armAddress, request.length);
							replyLength += request.length;
						}
						else if(kind == APBRIDGE_PACKET_WRITE)
						{
							armAddress = P2C(request.address);
							/* Preserve neighboring bytes on partial MEM1 cache lines. */
							sync_before_read_align32((void*)armAddress, request.length);
							memcpy((void*)armAddress,
								APBridge_Packet + APBRIDGE_WRITE_REQUEST_SIZE, request.length);
							sync_after_write_align32((void*)armAddress, request.length);
						}
						else
						{
							txResult = APBridge_TxCacheLookup(&APBridge_TxCache,
								&request, APBridge_Packet + APBRIDGE_TX_REQUEST_SIZE);
							if(txResult == APBRIDGE_TX_ID_CONFLICT)
								request.error = APBRIDGE_TX_CONFLICT;
							else if(txResult == APBRIDGE_TX_NEW)
							{
								armAddress = P2C(request.address);
								sync_before_read_align32((void*)armAddress, request.length);
								memcpy((void*)armAddress,
									APBridge_Packet + APBRIDGE_TX_REQUEST_SIZE, request.length);
								sync_after_write_align32((void*)armAddress, request.length);
								/* Cache the completed write before attempting WROK. */
								APBridge_TxCacheRecord(&APBridge_TxCache, &request,
									APBridge_Packet + APBRIDGE_TX_REQUEST_SIZE);
							}
						}
					}
					APBridge_EncodeMemoryReply(APBridge_ReadReply, &request, kind);
					APBridge_Vec[0].data = APBridge_ReadReply;
				}
				APBridge_Vec[0].len = replyLength;
				APBridge_Vec[1].data = &APBridge_Send;
				APBridge_Vec[1].len = sizeof(APBridge_Send);
				if(IOS_Ioctlv(fd, so_sendto, 2, 0, APBridge_Vec) != (s32)replyLength
					&& kind == APBRIDGE_PACKET_HELLO)
					return;
			}
		}
		/* Yield even under continuous valid or unknown traffic. */
		mdelay(10);
	}
}

static u32 APBridge_Thread(void *arg)
{
	s32 fd, socket;
	(void)arg;
	APBridge_Record(APBRIDGE_DIAG_THREAD_STARTED, 0);

	/* BBA SOStartup/SOCleanup owns global IOS network state. Separate device
	 * handles do not isolate it. Keep v0.2 dormant with BBA enabled rather
	 * than racing game startup/cleanup or changing existing BBA behavior.
	 */
	if(ConfigGetConfig(NIN_CFG_BBA_EMU))
		while(1) mdelay(1000);

	while(1)
	{
		fd = APBridge_StartNetwork();
		if(fd >= 0)
		{
			socket = APBridge_OpenSocket(fd);
			if(socket >= 0)
			{
				APBridge_Serve(fd, socket);
				APBridge_Params[0] = socket;
				IOS_Ioctl(fd, so_close, APBridge_Params, 4, NULL, 0);
			}
			IOS_Close(fd);
		}
		mdelay(APBRIDGE_RETRY_MS);
	}

	return 0;
}

void APBridge_Init(void)
{
	if(APBridge_Stack != NULL)
		return;
	/* malloca() shuts down Nintendont on failure; a bridge failure must not. */
	APBridge_Stack = (u32*)heap_alloc_aligned(0, APBRIDGE_STACK_SIZE, 32);
	if(APBridge_Stack == NULL)
		return;

	APBridge_ThreadID = (s32)thread_create(
		APBridge_Thread,
		NULL,
		APBridge_Stack,
		APBRIDGE_STACK_SIZE / sizeof(u32),
		APBRIDGE_THREAD_PRIORITY,
		1
	);

	if(APBridge_ThreadID < 0)
	{
		heap_free(0, APBridge_Stack);
		APBridge_Stack = NULL;
		return;
	}
	if(thread_continue(APBridge_ThreadID) < 0)
	{
		/* Free the stack only once IOS confirms the thread was cancelled. */
		if(thread_cancel(APBridge_ThreadID, 0) >= 0)
		{
			heap_free(0, APBridge_Stack);
			APBridge_Stack = NULL;
			APBridge_ThreadID = -1;
		}
	}
}
