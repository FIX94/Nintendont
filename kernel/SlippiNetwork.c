/* SlippiNetwork.c
 * Slippi thread for handling network transactions.
 */

#include "SlippiNetwork.h"
#include "SlippiDebug.h"
#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"
#include "ff_utf8.h"

// Thread stuff
static u32 SlippiNetwork_Thread;
extern char __slippi_network_stack_addr, __slippi_network_stack_size;
static u32 SlippiNetworkHandlerThread(void *arg);

// Connection variables
static struct sockaddr_in server __attribute__((aligned(32)));
static int server_sock __attribute__((aligned(32)));
static int client_sock __attribute__((aligned(32)));
static u32 client_alive_ts;

// Global network buffer state from kernel/Slippi.c
extern void *SlipMem;
extern u32 SlipMemWriteCursor;
extern u32 SlipMemSize;
u32 SlipMemReadCursor = 0;	// consumer's cursor into SlipMem

// Global network state
extern s32 top_fd;		// from kernel/net.c
u32 SlippiServerStarted = 0;	// used by kernel/main.c

/* Dispatch the server thread. This should only be run once in kernel/main.c
 * after NCDInit() has actually brought up the networking stack and we have
 * some connectivity. */
void SlippiNetworkShutdown() { thread_cancel(SlippiNetwork_Thread, 0); }
s32 SlippiNetworkInit()
{
	server_sock = -1;
	client_sock = -1;

	dbgprintf("net_thread is starting ...\r\n");
	SlippiNetwork_Thread = do_thread_create(
		SlippiNetworkHandlerThread,
		((u32*)&__slippi_network_stack_addr),
		((u32)(&__slippi_network_stack_size)),
		0x78
	);
	thread_continue(SlippiNetwork_Thread);
	SlippiServerStarted = 1;
	ppc_msg("SERVER INIT OK\x00", 15);
	return 0;
}

/* Create a new socket for the server to bind and listen on. */
s32 startServer() {
	s32 res;

	server_sock = socket(top_fd, AF_INET, SOCK_STREAM, IPPROTO_IP);
	dbgprintf("server_sock: %d\r\n", server_sock);

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = 666;
	server.sin_addr.s_addr = INADDR_ANY;

	res = bind(top_fd, server_sock, (struct sockaddr*)&server);
	if (res < 0) {
		close(top_fd, server_sock);
		server_sock = -1;
		dbgprintf("bind() failed with: %d\r\n", res);
		return res;
	}
	res = listen(top_fd, server_sock, 1);
	if (res < 0)  {
		close(top_fd, server_sock);
		server_sock = -1;
		dbgprintf("listen() failed with: %d\r\n", res);
		return res;
	}
	return 0;
}

/* Accept a client */
void listenForClient()
{
	if (client_sock < 0)
	{
		client_sock = accept(top_fd, server_sock);
		if (client_sock >= 0) {
			ppc_msg("CLIENT OK\x00", 10);
			client_alive_ts = read32(HW_TIMER);
		}
	}
}

/* Deal with sending Slippi data over the network. */
u32 handleFileTransfer(u32 valid_bytes) {
	s32 res;
	u32 tx_size, chunk_size;

	if (valid_bytes == 0) return 0;

	tx_size = (valid_bytes >= MAX_TX_SIZE) ? MAX_TX_SIZE : valid_bytes;
	res = sendto(top_fd, client_sock, (SlipMem + SlipMemReadCursor),
			tx_size, 0);

	// Naive client hangup detection
	if (res < 0) {
		close(top_fd, client_sock);
		client_sock = -1;
		client_alive_ts = 0;
		return res;
	}

	// Otherwise, keep track of the bytes we've consumed in the buffer
	if (res == tx_size) {
		SlipMemReadCursor = (SlipMemReadCursor + tx_size) % SlipMemSize;
		return res;
	}
}

/* Return the status of the networking thread. */
int getConnectionStatus() {
	if (server_sock < 0)
		return CONN_STATUS_NO_SERVER;
	if (client_sock < 0)
		return CONN_STATUS_NO_CLIENT;
	else if (client_sock >= 0)
		return CONN_STATUS_CONNECTED;

	return CONN_STATUS_UNKNOWN;
}

/* Give some naive indication of client hangup. If sendto() returns some error,
 * this probably indicates that we can stop talking to the current client */
static char alive_msg[] __attribute__((aligned(32))) = "HELO";
s32 check_alive(void)
{
	s32 res;
	res = sendto(top_fd, client_sock, alive_msg, sizeof(alive_msg), 0);

	if (res == sizeof(alive_msg)) {
		client_alive_ts = read32(HW_TIMER);
		return 0;
	}
	else if (res <= 0) {
		client_alive_ts = 0;
		close(top_fd, client_sock);
		client_sock = -1;
		return -1;
	}
}

/* This is the main loop for the server.
 *   - Only transmit when there's some data left in SlipMem
 *   - When there's no valid data, periodically send some keep-alive
 *     messages to the client so we can determine if they've hung up
 */
static u32 SlippiNetworkHandlerThread(void *arg) {
	int status;
	u32 valid_bytes = 0;

	while (1)
	{
		status = getConnectionStatus();
		switch (status)
		{
		case CONN_STATUS_NO_SERVER:
			startServer();
			break;
		case CONN_STATUS_NO_CLIENT:
			listenForClient();
			break;
		case CONN_STATUS_CONNECTED:
			valid_bytes = (SlipMemWriteCursor - SlipMemReadCursor) % SlipMemSize;
			if (valid_bytes == 0) {
				if (TimerDiffSeconds(client_alive_ts) > 3)
					check_alive();
				break;
			}
			if (valid_bytes > 0)
				handleFileTransfer(valid_bytes);
			break;
		}

		mdelay(100);
	}

	return 0;
}
