
#include "global.h"
#include "common.h"
#include "net.h"
#include "string.h"

#include "SlippiNetwork.h"

#define THREAD_CYCLE_TIME_MS	100

// Measured in seconds
#define BROADCAST_PERIOD	10

static u32 SlippiNetworkBroadcast_Thread;
extern char __slippi_network_broadcast_stack_addr; 
extern char __slippi_network_broadcast_stack_size;
static u32 SlippiNetworkBroadcastHandlerThread(void *arg);

extern s32 top_fd;		// from kernel/net.c

// UDP message contents to-be-sent to the broadcast address
static char ready_msg[10] ALIGNED(32) = "SLIP_READY";

static int discover_sock ALIGNED(32);
static struct sockaddr_in discover ALIGNED(32);

static u32 broadcast_ts;
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



/* Set up a socket and connect() to the local broadcast address. */ 
s32 startBroadcast()
{
	s32 res;

	discover_sock = socket(top_fd, AF_INET, SOCK_DGRAM, IPPROTO_IP);
	memset(&discover, 0, sizeof(struct sockaddr_in));
	discover.sin_family = AF_INET;
	discover.sin_len = sizeof(struct sockaddr_in);
	discover.sin_port = 667;
	discover.sin_addr.s_addr = 0xffffffff;

	// Start indicating our status to clients on the local network
	//res = connect(top_fd, discover_sock, (struct address *)&discover);
	res = connect(top_fd, discover_sock, (struct sockaddr *)&discover);
	sendto(top_fd, discover_sock, ready_msg, sizeof(ready_msg), 0);

	// Update the broadcast message timer
	broadcast_ts = read32(HW_TIMER);

	return res;
}


s32 do_broadcast(void)
{
	s32 res;

	if (TimerDiffSeconds(broadcast_ts) < BROADCAST_PERIOD)
		return 0;

	res = sendto(top_fd, discover_sock, ready_msg, sizeof(ready_msg), 0);
	broadcast_ts = read32(HW_TIMER);

	return res;
}



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

