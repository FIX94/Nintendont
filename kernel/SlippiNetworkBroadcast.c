/* kernel/SlippiNetworkBroadcast.c
 * Slippi thread for handling broadcast messages on the local network.
 */

#include "global.h"
#include "common.h"
#include "net.h"
#include "string.h"

#include "SlippiNetwork.h"

#define THREAD_CYCLE_TIME_MS	5000

// Measured in seconds
#define BROADCAST_PERIOD	10

// Thread state
static u32 SlippiNetworkBroadcast_Thread;
extern char __slippi_network_broadcast_stack_addr;
extern char __slippi_network_broadcast_stack_size;
static u32 SlippiNetworkBroadcastHandlerThread(void *arg);

// Global state from kernel/net.c
extern s32 top_fd;
extern u8 wifi_mac_address[6];

// String for Slippi console nickname; probably from config?
char slippi_nickname[32] ALIGNED(32);

// Broadcast message structure
const char slip_ready[10] ALIGNED(32) = "SLIP_READY";
static struct broadcast_msg ready_msg ALIGNED(32);
struct broadcast_msg
{
	unsigned char	cmd[10];
	u8		mac_addr[6];	// Wi-Fi interface MAC address
	unsigned char	nickname[32];	// Console nickname
} __attribute__((packed));

// Broadcast socket state
static int discover_sock ALIGNED(32);
static u32 broadcast_ts;
static struct sockaddr_in discover ALIGNED(32) = {
	.sin_family	= AF_INET,
	.sin_port	= 20582,
	{
		.s_addr	= 0xffffffff,
	},
};


void reset_broadcast_timer(void) { broadcast_ts = read32(HW_TIMER); }
void SlippiNetworkBroadcastShutdown() { thread_cancel(SlippiNetworkBroadcast_Thread, 0); }
s32 SlippiNetworkBroadcastInit()
{
	SlippiNetworkBroadcast_Thread = do_thread_create(
		SlippiNetworkBroadcastHandlerThread,
		((u32 *)&__slippi_network_broadcast_stack_addr),
		((u32)(&__slippi_network_broadcast_stack_size)),
		0x78);
	thread_continue(SlippiNetworkBroadcast_Thread);
	return 0;
}


/* startBroadcast()
 * Set up socket, connect to local broadcast address, send first message.
 */
s32 startBroadcast()
{
	s32 res;

	// Prepare broadcast message buffer with console info
	_sprintf(slippi_nickname, "%s", "slippi-console-1");
	memcpy(&ready_msg.cmd, &slip_ready, sizeof(slip_ready));
	memcpy(&ready_msg.mac_addr, &wifi_mac_address, sizeof(wifi_mac_address));
	memcpy(&ready_msg.nickname, &slippi_nickname, sizeof(slippi_nickname));

	discover_sock = socket(top_fd, AF_INET, SOCK_DGRAM, IPPROTO_IP);

	// Start indicating our status to clients on the local network
	res = connect(top_fd, discover_sock, (struct sockaddr *)&discover);
	sendto(top_fd, discover_sock, &ready_msg, sizeof(ready_msg), 0);

	// Initialize the broadcast message timer
	broadcast_ts = read32(HW_TIMER);

	return res;
}


/* do_broadcast()
 * Broadcast a SLIP_READY message on the local network.
 */
s32 do_broadcast(void)
{
	s32 res;

	if (TimerDiffSeconds(broadcast_ts) < BROADCAST_PERIOD)
		return 0;

	res = sendto(top_fd, discover_sock, &ready_msg, sizeof(ready_msg), 0);
	broadcast_ts = read32(HW_TIMER);
	return res;
}


/* SlippiNetworkBroadcastHandlerThread()
 * Main loop for the broadcast thread.
 */
static u32 SlippiNetworkBroadcastHandlerThread(void *arg)
{
	int status;
	startBroadcast();

	while (1)
	{
		status = getConnectionStatus();
		if (status != CONN_STATUS_CONNECTED)
			do_broadcast();
		mdelay(THREAD_CYCLE_TIME_MS);
	}
	return 0;
}
