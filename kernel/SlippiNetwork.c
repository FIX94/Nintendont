#include "SlippiNetwork.h"
#include "net.h"
#include "ff_utf8.h"

// Thread stuff
static u32 SlippiNetwork_Thread;
extern char __slippi_network_stack_addr, __slippi_network_stack_size;
static u32 SlippiNetworkHandlerThread(void *arg);

// Connection variables
static struct sockaddr_in server __attribute__((aligned(32)));
static int sock __attribute__((aligned(32)));
int client_sock __attribute__((aligned(32)));
s32 top_fd __attribute__((aligned(32)));

// File transfer variable
FIL currentFile;
char currentFilePath[50];
u32 currentFilePos = 0;

// Defines
#define CONN_STATUS_UNKNOWN 0
#define CONN_STATUS_NO_SERVER 1
#define CONN_STATUS_NO_CLIENT 2
#define CONN_STATUS_CONNECTED 3 

#define NETWORK_TRANSFER_BUF_SIZE 1024

s32 SlippiNetworkInit()
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

	dbgprintf("net_thread is starting ...\r\n");
	SlippiNetwork_Thread = do_thread_create(
		SlippiNetworkHandlerThread,
		((u32*)&__slippi_network_stack_addr),
		((u32)(&__slippi_network_stack_size)),
		0x78
	);
	thread_continue(SlippiNetwork_Thread);

	return 0;
}

void SlippiShutdown()
{
	thread_cancel(SlippiNetwork_Thread, 0);
}

int getConnectionStatus() {
	// This function will return the current status of the connection,
	// this is used to determine what action to take.
	// States are as follows:
	// 0 - unknown state
	// 1 - server socket is closed
	// 2 - waiting for client connection
	// 3 - connected to client
	if (sock < 0) {
		return CONN_STATUS_NO_SERVER;
	}

	if (client_sock < 0) {
		return CONN_STATUS_NO_CLIENT;
	} else {
		return CONN_STATUS_CONNECTED;
	}

	return CONN_STATUS_UNKNOWN;
}

s32 startServer() {
	s32 res;

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
}

void listenForClient() {
	client_sock = accept(top_fd, sock);
}

void handleConnection() {
	int status = getConnectionStatus();
	switch (status) {
		case 1:
			startServer();
			break;
		case 2:
			listenForClient();
			break;
	}
}

void openFileToTransfer() {
	// Figure out when a new file is created and move transfer to that file
	// TODO: Make sure that previous file is done transfering before moving
	FRESULT fileOpenResult = f_open_char(&currentFile, currentFilePath, FA_OPEN_EXISTING | FA_READ);

	if (fileOpenResult != FR_OK) {
		return;
	}
}

void handleFileTransfer() {
	int status = getConnectionStatus();
	if (status != CONN_STATUS_CONNECTED) {
		// Don't do anything if not connected to a client
		return;
	}

	if (f_eof(&currentFile)) {
		// Don't do anything if end of file
		return;
	}

	static u8 dataBuf[NETWORK_TRANSFER_BUF_SIZE];
	u32 dataBufByteCount = 0;

	// 1) Load data from file to fill our buffer
	UINT readCount;
	FRESULT fileReadResult = f_read(&currentFile, &dataBuf, NETWORK_TRANSFER_BUF_SIZE, &readCount);
	if (fileReadResult != FR_OK || readCount <= 0) {
		// Nothing to transfer
		return;
	}

	// 2) Emit data to client (assuming they've connected already)
	sendto(top_fd, client_sock, dataBuf, readCount, 0);
}

static u32 SlippiNetworkHandlerThread(void *arg) {
	while (1) {
		handleConnection();
		openFileToTransfer();
		handleFileTransfer();
		mdelay(100);
	}
}
