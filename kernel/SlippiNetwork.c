/* SlippiNetwork.c
 * Slippi thread for handling network transactions.
 */

#include "SlippiNetwork.h"
#include "SlippiDebug.h"
#include "SlippiMemory.h"
#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"
#include "ff_utf8.h"

#include "SlippiNetworkBroadcast.h"

// Game can transfer at most 784 bytes / frame
// That means 4704 bytes every 100 ms. Let's aim to handle
// double that, making our read buffer 10000 bytes for 100 ms.
// The cycle time was lowered to 11 ms (sendto takes about 10ms
// on average), Because of this I lowered the buffer from what
// it needed to be at 100 ms
#define READ_BUF_SIZE 2500
#define THREAD_CYCLE_TIME_MS 1

// Thread stuff
static u32 SlippiNetwork_Thread;
extern char __slippi_network_stack_addr, __slippi_network_stack_size;
static u32 SlippiNetworkHandlerThread(void *arg);

// Server state
static int server_sock __attribute__((aligned(32)));
static struct sockaddr_in server __attribute__((aligned(32))) = {
	.sin_family	= AF_INET,
	.sin_port	= 666,
	{
		.s_addr	= INADDR_ANY,
	},
};

// Client state
static int client_sock __attribute__((aligned(32)));
static u32 client_alive_ts;

// Global network state
extern s32 top_fd;		// from kernel/net.c
u32 SlippiServerStarted = 0;	// used by kernel/main.c


/* SlippiNetworkInit()
 * Dispatch the server thread. This should only be run once in kernel/main.c
 * after NCDInit() has actually brought up the networking stack and we have
 * some connectivity.
 */
void SlippiNetworkShutdown() { thread_cancel(SlippiNetwork_Thread, 0); }
s32 SlippiNetworkInit()
{
	server_sock = -1;
	client_sock = -1;

	dbgprintf("net_thread is starting ...\r\n");
	SlippiNetwork_Thread = do_thread_create(
		SlippiNetworkHandlerThread,
		((u32 *)&__slippi_network_stack_addr),
		((u32)(&__slippi_network_stack_size)),
		0x78);
	thread_continue(SlippiNetwork_Thread);
	SlippiServerStarted = 1;
	ppc_msg("SERVER INIT OK\x00", 15);
	return 0;
}


/* startServer()
 * Create a new server socket, bind, then start listening.
 */
s32 startServer()
{
	s32 res;

	server_sock = socket(top_fd, AF_INET, SOCK_STREAM, IPPROTO_IP);
	dbgprintf("server_sock: %d\r\n", server_sock);

	res = bind(top_fd, server_sock, (struct sockaddr *)&server);
	if (res < 0)
	{
		close(top_fd, server_sock);
		server_sock = -1;
		dbgprintf("bind() failed with: %d\r\n", res);
		return res;
	}
	
	res = listen(top_fd, server_sock, 1);
	if (res < 0)
	{
		close(top_fd, server_sock);
		server_sock = -1;
		dbgprintf("listen() failed with: %d\r\n", res);
		return res;
	}
	return 0;
}


/* listenForClient()
 * Check the status of the client socket and potentially accept a client.
 */
void listenForClient()
{
	// We already have a client
	if (client_sock >= 0)
		return;

	// Try to accept a client
	client_sock = accept(top_fd, server_sock);
	if (client_sock >= 0)
	{
		int flags = 1;
		s32 optRes = setsockopt(top_fd, client_sock, IPPROTO_TCP,
			TCP_NODELAY, (void *)&flags, sizeof(flags));
		dbgprintf("[TCP_NODELAY] Client setsockopt result: %d\r\n", optRes);

		dbgprintf("Client connection detected\r\n");
		ppc_msg("CLIENT OK\x00", 10);
		client_alive_ts = read32(HW_TIMER);
	}
}


/* handleFileTransfer()
 * Deal with sending Slippi data over the network:
 *
 *	1. Attempt read from Slippi buffer
 *	2. If we consume some data from buffer, send it to the client
 *	3. If sendto() isn't successful, hang up on a client
 *	4. Update our read cursor in Slippi buffer
 */
static u8 readBuf[READ_BUF_SIZE];
static u64 memReadPos = 0;
static SlpGameReader reader;
s32 handleFileTransfer()
{
	int status = getConnectionStatus();
	if (status != CONN_STATUS_CONNECTED) {
		// Do nothing if we aren't connected to a client
		return 0;
	}

	SlpMemError err = SlippiMemoryRead(&reader, readBuf, READ_BUF_SIZE, memReadPos);
	if (err)
	{
		if (err == SLP_READ_OVERFLOW)
		{
			memReadPos = SlippiRestoreReadPos();
			dbgprintf("WARN: Overflow read error detected. Reset to: %X\r\n", memReadPos);
		}
		mdelay(1000);

		// For specific errors, bytes will still be read. Not returning to deal with those
	}

	// dbgprintf("Checking if there's data to transfer...\r\n");

	if (reader.lastReadResult.bytesRead == 0)
		return 0;

	// sendto takes an average of around 10 ms to return. Seems to range from 3-30 ms or so
	s32 res = sendto(top_fd, client_sock, readBuf, reader.lastReadResult.bytesRead, 0);

	// Naive client hangup detection
	if (res < 0)
	{
		dbgprintf("Client disconnect detected\r\n");
		close(top_fd, client_sock);
		client_sock = -1;
		client_alive_ts = 0;
		ppc_msg("CLIENT HUP\x00", 11);
		return res;
	}

	// Indicate client still active
	client_alive_ts = read32(HW_TIMER);

	// Only update read position if transfer was successful
	memReadPos += reader.lastReadResult.bytesRead;

	return 0;
}


/* getConnectionStatus()
 * Return the status of the networking thread.
 */
int getConnectionStatus()
{
	if (server_sock < 0)
		return CONN_STATUS_NO_SERVER;
	if (client_sock < 0)
		return CONN_STATUS_NO_CLIENT;
	else if (client_sock >= 0)
		return CONN_STATUS_CONNECTED;

	return CONN_STATUS_UNKNOWN;
}


/* checkAlive()
 * Give some naive indication of whether or not a client has hung up. If our
 * sendto() here returns some error, this probably indicates that we can stop
 * talking to the current client and reset the socket.
 */
static char alive_msg[] __attribute__((aligned(32))) = "HELO";
s32 checkAlive(void)
{
	int status = getConnectionStatus();
	if (status != CONN_STATUS_CONNECTED) {
		// Do nothing if we aren't connected to a client
		// the handleFileTransfer could sometimes cause a disconnect
		return 0;
	}

	if (TimerDiffSeconds(client_alive_ts) < 3)
	{
		// Only check alive if we haven't detected any communication
		return 0;
	}

	s32 res;
	res = sendto(top_fd, client_sock, alive_msg, sizeof(alive_msg), 0);

	if (res == sizeof(alive_msg))
	{
		client_alive_ts = read32(HW_TIMER);
		// 250 ms wait. The goal here is that the keep alive message
		// will be sent by itself without anything following it
		mdelay(250);

		return 0;
	}
	else if (res <= 0)
	{
		dbgprintf("Client disconnect detected\r\n");
		client_alive_ts = 0;
		close(top_fd, client_sock);
		client_sock = -1;
		reset_broadcast_timer();
		ppc_msg("CLIENT HUP\x00", 11);
		return -1;
	}

	return 0;
}


/* SlippiNetworkHandlerThread()
 * This is the main loop for the server.
 *   - Initialize the server
 *   - Accept a client (if we aren't already tracking one)
 *   - Only transmit to client when there's some data left in SlipMem
 *   - When there's no valid data, periodically send some keep-alive
 *     messages to the client so we can determine if they've hung up
 */
static u32 SlippiNetworkHandlerThread(void *arg)
{
	while (1)
	{
		int status = getConnectionStatus();
		switch (status)
		{
		case CONN_STATUS_NO_SERVER:
			startServer();
			break;
		case CONN_STATUS_NO_CLIENT:
			listenForClient();
			break;
		case CONN_STATUS_CONNECTED:
			handleFileTransfer();
			checkAlive();
			break;
		}

		mdelay(THREAD_CYCLE_TIME_MS);
	}

	return 0;
}
