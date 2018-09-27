#include "SlippiNetwork.h"
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
static int sock __attribute__((aligned(32)));
static int client_sock __attribute__((aligned(32)));

// Global network state
extern s32 top_fd; 
u32 SlippiServerStarted = 0; // Used by kernel/main.c

// File transfer variable
#define NETWORK_FILE_PATH_LEN 50

extern FIL currentFile; // use current file from kernel/Slippi.c

char currentFilePath[NETWORK_FILE_PATH_LEN];
FSIZE_t currentFilePos = 0;

char nextFilePath[NETWORK_FILE_PATH_LEN];
bool newNextFile = false;

// Defines
#define CONN_STATUS_UNKNOWN 0
#define CONN_STATUS_NO_SERVER 1
#define CONN_STATUS_NO_CLIENT 2
#define CONN_STATUS_CONNECTED 3 

#define NETWORK_TRANSFER_BUF_SIZE 1024


/* This should only be dispatched once in kernel/main.c after NCDInit() has
 * actually brought up the networking stack and we have connectivity. */
s32 SlippiNetworkInit()
{
	sock = -1;
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

	return 0;
}

void SlippiNetworkShutdown()
{
	thread_cancel(SlippiNetwork_Thread, 0);
}


void SlippiNetworkSetNewFile(const char* path) {
	memcpy(&nextFilePath[0], path, NETWORK_FILE_PATH_LEN);
	newNextFile = true;
}

/* Return the current status of the connection. 
 * Used to determine what action the server thread should take. 
 * States are as follows:
 *
 *	0 - unknown state
 *	1 - server socket is closed
 *	2 - waiting for client connection
 *	3 - connected to client
 */
int getConnectionStatus() {
	if (sock < 0)
		return CONN_STATUS_NO_SERVER;

	if (client_sock < 0)
		return CONN_STATUS_NO_CLIENT;
	else
		return CONN_STATUS_CONNECTED;

	return CONN_STATUS_UNKNOWN;
}

/* Get a new socket, then bind and listen on it */
s32 startServer() {
	s32 res;

	sock = socket(top_fd, AF_INET, SOCK_STREAM, IPPROTO_IP);
	dbgprintf("sock: %d\r\n", sock);

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = 666;
	server.sin_addr.s_addr = INADDR_ANY;

	res = bind(top_fd, sock, (struct sockaddr*)&server);
	if (res < 0) {
		close(top_fd, sock);
		sock = -1;
		dbgprintf("bind() failed with: %d\r\n", res);
		return res;
	}

	res = listen(top_fd, sock, 1);
	if (res < 0)  {
		close(top_fd, sock);
		sock = -1;
		dbgprintf("listen() failed with: %d\r\n", res);
		return res;
	}

	dbgprintf("Server started!\r\n");

	return 0;
}

static u8 acceptMsg[4] __attribute__((aligned(32))) = "OK\x00\x00";
void listenForClient() {
	if (client_sock < 0) 
	{
		client_sock = accept(top_fd, sock);
		if (client_sock >= 0) {
			dbgprintf("Client connected!\r\n");
			sendto(top_fd, client_sock, &acceptMsg[0], 4, 0);
		}
	}
	else 
	{
		mdelay(10);
	}
}

void handleConnection() {
	int status = getConnectionStatus();
	switch (status) {
		case CONN_STATUS_NO_SERVER:
			startServer();
			break;
		case CONN_STATUS_NO_CLIENT:
			listenForClient();
			break;
		default:
			mdelay(10);
			break;
	}
}

void openNextFile() {
	// Figure out when a new file is created and move transfer to that file
	if (!newNextFile) {
		// If there is no new file, we don't need to open anything
		return;
	}

	// If we are not at the end of the current file, don't switch yet
	if (f_eof(&currentFile) == 0) {
		return;
	}

	memcpy(&currentFilePath[0], nextFilePath, NETWORK_FILE_PATH_LEN);

	dbgprintf("reading from new file: %s\r\n", currentFilePath);

	// Open file
	FRESULT fileOpenResult = f_open_char(&currentFile, currentFilePath, FA_OPEN_EXISTING | FA_READ);
	if (fileOpenResult != FR_OK) {
		return;
	}

	// Indicate we have loaded the new file
	newNextFile = false;
}

void openFileToTransfer() {
	//TODO: Add function here to reconnect to file if connection is severed

	// This function will open a new file if there is a next file to deal with
	openNextFile();
}

static u8 dataBuf[NETWORK_TRANSFER_BUF_SIZE];
void handleFileTransfer() {
	int status = getConnectionStatus();

	// Do nothing if (a) there's no client, or (b) we're at EOF and need
	// to wait for the file to grow
	if ( (status != CONN_STATUS_CONNECTED) || (currentFilePos >= f_size(&currentFile)) )
		return;


	// 1) Load data from file to fill our buffer
	UINT readCount = 0;
	//memset(&dataBuf, 0x00, NETWORK_TRANSFER_BUF_SIZE);

	f_lseek(&currentFile, currentFilePos); // Use our cursor
	FRESULT fileReadResult = f_read(&currentFile, &dataBuf, NETWORK_TRANSFER_BUF_SIZE, &readCount);

	// Nothing to transfer: don't send anything
	if (fileReadResult != FR_OK || readCount <= 0) {
		// reset cursor to EOF
		dbgprintf("handleFileTransfer: not OK\r\n");
		f_lseek(&currentFile, f_size(&currentFile)); 
		return;
	}

	// 2) Emit data to client and increment our offset into the file
	dbgprintf("sending data!\r\n");
	sendto(top_fd, client_sock, dataBuf, readCount, 0);
	currentFilePos += readCount;
}

static u32 SlippiNetworkHandlerThread(void *arg) {
	while (1) {
		handleConnection();
		//openFileToTransfer();
		handleFileTransfer();

		mdelay(100);
	}

	return 0;
}
