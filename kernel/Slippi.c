#include "Slippi.h"
#include "alloc.h"
#include "debug.h"
#include "string.h"
#include "ff_utf8.h"
#include "DI.h"

#include "net.h"

#define RECEIVE_BUFFER_SIZE 1000 // Must be longer than the games' transfer buffer (currently 784)
#define FRAME_PAYLOAD_BUFFER_SIZE 0x80
#define WRITE_BUFFER_LENGTH 0x1000
#define PAYLOAD_SIZES_BUFFER_SIZE 20
#define FOOTER_BUFFER_LENGTH 200

// Global state from net.c 
extern int client_sock;
extern s32 top_fd;

static u32 SlippiHandlerThread(void *arg);

enum
{
	CMD_UNKNOWN = 0x0,
	CMD_RECEIVE_COMMANDS = 0x35,
	CMD_RECEIVE_GAME_INFO = 0x36,
	CMD_RECEIVE_PRE_FRAME_UPDATE = 0x37,
	CMD_RECEIVE_POST_FRAME_UPDATE = 0x38,
	CMD_RECEIVE_GAME_END = 0x39,
};

// Thread stuff
static u32 Slippi_Thread;
static void *Slippi_MessageHeap;
static u32 Slippi_MessageQueue;
extern char __slippi_stack_addr, __slippi_stack_size;

typedef struct BufferAccess {
	u8 buffer[WRITE_BUFFER_LENGTH]; // Data to write
	struct ipcmessage message;
} bufferAccess;

#define BUFFER_ACCESS_COUNT 35 // need lots of buffers for 4 ICs. 37 is max
static bufferAccess accessManager[BUFFER_ACCESS_COUNT];
static u32 writeBufferIndex = 0;

// File writing stuff
static u32 fileIndex = 1;

// vars for metadata generation
u32 gameStartTime;
s32 lastFrame;

// Payload Sizes
static u16 payloadSizes[PAYLOAD_SIZES_BUFFER_SIZE];

void SlippiInit()
{
	// Set the commands payload to start at length 1. The reason for this
	// is that the game will pass in all the command sizes but if
	// it starts at 0 then the command is ignored and nothing ever happens
	payloadSizes[0] = 1;

	Slippi_MessageHeap = malloca(sizeof(void*) * BUFFER_ACCESS_COUNT, 0x20);
	Slippi_MessageQueue = mqueue_create(Slippi_MessageHeap, BUFFER_ACCESS_COUNT);
	Slippi_Thread = do_thread_create(SlippiHandlerThread, ((u32*)&__slippi_stack_addr), ((u32)(&__slippi_stack_size)), 0x78);
	thread_continue(Slippi_Thread);
}

void SlippiShutdown()
{
	mqueue_destroy(Slippi_MessageQueue);
	free(Slippi_MessageHeap);
	thread_cancel(Slippi_Thread, 0);
}

//we cant include time.h so hardcode what we need
struct tm
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
};
extern struct tm *gmtime(u32 *time);

char *generateFileName(bool isNewFile)
{
	// // Add game start time
	// u8 dateTimeStrLength = sizeof "20171015T095717";
	// char *dateTimeBuf = (char *)malloc(dateTimeStrLength);
	// strftime(&dateTimeBuf[0], dateTimeStrLength, "%Y%m%dT%H%M%S", localtime(&gameStartTime));

	// std::string str(&dateTimeBuf[0]);
	// return StringFromFormat("Slippi/Game_%s.slp", str.c_str());

	static char pathStr[50];
	struct tm *tmp = gmtime(&gameStartTime);

	_sprintf(
		&pathStr[0], "usb:/Slippi/Game_%04d%02d%02dT%02d%02d%02d.slp", tmp->tm_year + 1900,
		tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec
	);

	if (isNewFile) {
		fileIndex += 1;
	}

	return pathStr;
}

u16 getPayloadSize(u8 command)
{
	int payloadSizesIndex = command - CMD_RECEIVE_COMMANDS;
	if (payloadSizesIndex >= PAYLOAD_SIZES_BUFFER_SIZE || payloadSizesIndex < 0)
	{
		return 0;
	}

	return payloadSizes[payloadSizesIndex];
}

void configureCommands(u8 *payload, u8 length)
{
	int i = 1;
	while (i < length)
	{
		// Go through the receive commands payload and set up other commands
		u8 commandByte = payload[i];
		u16 commandPayloadSize = payload[i + 1] << 8 | payload[i + 2];
		payloadSizes[commandByte - CMD_RECEIVE_COMMANDS] = commandPayloadSize;

		// dbgprintf("Index: 0x%02X, Size: 0x%02X\r\n", commandByte - CMD_RECEIVE_COMMANDS, commandPayloadSize);

		i += 3;
	}
}

void updateMetadataFields(u8* payload, u32 length) {
	if (length <= 0 || payload[0] != CMD_RECEIVE_POST_FRAME_UPDATE) {
		// Only need to update if this is a post frame update
		return;
	}

	// TODO: Consider moving this stuff into a message, feels wrong
	// TODO: using this data in the thread without communicating through
	// TODO: the buffers

	// Keep track of last frame
	lastFrame = payload[1] << 24 | payload[2] << 16 | payload[3] << 8 | payload[4];

	// TODO: Add character usage
	// Keep track of character usage
	// u8 playerIndex = payload[5];
	// u8 internalCharacterId = payload[7];
	// if (!characterUsage.count(playerIndex) || !characterUsage[playerIndex].count(internalCharacterId)) {
	// 	characterUsage[playerIndex][internalCharacterId] = 0;
	// }
	// characterUsage[playerIndex][internalCharacterId] += 1;
}

void writeHeader(FIL *file) {
	u8 header[] = { '{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0 };

	u32 wrote;
	f_write(file, header, sizeof(header), &wrote);
	f_sync(file);
}

void completeFile(FIL *file, u32 writtenByteCount) {
	u8 footer[FOOTER_BUFFER_LENGTH];
	u32 writePos = 0;

	// Write opener
	u8 footerOpener[] = { 'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{' };
	u8 writeLen = sizeof(footerOpener);
	memcpy(&footer[writePos], footerOpener, writeLen);
	writePos += writeLen;

	// Write startAt
	// TODO: Figure out how to specify time zone
	char timeStr[] = "2011-10-08T07:07:09";
	struct tm *tmp = gmtime(&gameStartTime);
	_sprintf(
		&timeStr[0], "%04d-%02d-%02dT%02d:%02d:%02d", tmp->tm_year + 1900,
		tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec
	);
	u8 startAtOpener[] = { 'U', 7, 's', 't', 'a', 'r', 't', 'A', 't', 'S', 'U', (u8)sizeof(timeStr) };
	writeLen = sizeof(startAtOpener);
	memcpy(&footer[writePos], startAtOpener, writeLen);
	writePos += writeLen;
	writeLen = sizeof(timeStr);
	memcpy(&footer[writePos], timeStr, writeLen);
	writePos += writeLen;

	// Write lastFrame
	u8 lastFrameOpener[] = { 'U', 9, 'l', 'a', 's', 't', 'F', 'r', 'a', 'm', 'e', 'l' };
	writeLen = sizeof(lastFrameOpener);
	memcpy(&footer[writePos], lastFrameOpener, writeLen);
	writePos += writeLen;
	memcpy(&footer[writePos], &lastFrame, 4);
	writePos += 4;

	// Write closing
	u8 closing[] = {
		'U', 7, 'p', 'l', 'a', 'y', 'e', 'r', 's', '{', '}',
		'U', 8, 'p', 'l', 'a', 'y', 'e', 'd', 'O', 'n', 'S', 'U',
		10, 'n', 'i', 'n', 't', 'e', 'n', 'd', 'o', 'n', 't',
		'}', '}'
	};
	writeLen = sizeof(closing);
	memcpy(&footer[writePos], closing, writeLen);
	writePos += writeLen;

	// Write footer
	u32 wrote;
	f_write(file, footer, writePos, &wrote);
	f_sync(file);

	f_lseek(file, 11);
	f_write(file, &writtenByteCount, 4, &wrote);
	f_sync(file);
}

void processPayload(u8 *payload, u32 length, u8 fileOption)
{
	bufferAccess *currentBuffer = &accessManager[writeBufferIndex];
	if (currentBuffer->message.result == -1) {
		dbgprintf("Slippi: buffer not processed fast enough: %d\r\n", writeBufferIndex);
	}

	if (fileOption > 0) {
		currentBuffer->message.ioctl.command = fileOption;
	}

	// Copy data to write buffer
	u32 writePos = currentBuffer->message.ioctl.length_io;
	memcpy(&currentBuffer->buffer[writePos], payload, length);
	currentBuffer->message.ioctl.length_io += length;
	
	updateMetadataFields(payload, length);

	// If write buffer is not full yet, don't do anything else
	bool isBufferFull = currentBuffer->message.ioctl.length_io >= WRITE_BUFFER_LENGTH - FRAME_PAYLOAD_BUFFER_SIZE;
	bool isGameComplete = fileOption == 2;
	bool isGameStart = payload[0] == CMD_RECEIVE_GAME_INFO;
	bool shouldWrite = isBufferFull || isGameComplete || isGameStart;
	if (!shouldWrite)
		return;

	// The cast on the next line is only to make the compiler happy
	currentBuffer->message.ioctl.buffer_io = (unsigned int *)currentBuffer->buffer;
	currentBuffer->message.result = -1;
	mqueue_send(Slippi_MessageQueue, &currentBuffer->message, 1);

	writeBufferIndex = (writeBufferIndex + 1) % BUFFER_ACCESS_COUNT;
}

void SlippiDmaWrite(const void *buf, u32 len) {
	static u8 receiveBuf[RECEIVE_BUFFER_SIZE];

	sync_before_read((void*)buf, len);
	memcpy(&receiveBuf[0], buf, len);

	// dbgprintf("Received buf with len: %d\r\n", len);

	u32 bufLoc = 0;

	u8 command = receiveBuf[0];
	if (command == CMD_RECEIVE_COMMANDS) {
		gameStartTime = GetCurrentTime();
		u8 receiveCommandsLen = receiveBuf[1];
		configureCommands(&receiveBuf[1], receiveCommandsLen);
		processPayload(&receiveBuf[0], receiveCommandsLen + 1, 1);
		bufLoc += receiveCommandsLen + 1;
	}
	
	// Handle payloads
	while (bufLoc < len) {
		command = receiveBuf[bufLoc];
		u16 payloadLen = getPayloadSize(command);
		if (payloadLen == 0) {
			// This should never happen, do something else if it does?
			return;
		}

		// dbgprintf("Processing buf at %d with len %d and command 0x%02X\r\n", bufLoc, payloadLen, command);

		// process this payload and close file if this is a game end command
		u8 fileOption = command == CMD_RECEIVE_GAME_END ? 2 : 0;
		processPayload(&receiveBuf[bufLoc], payloadLen + 1, fileOption);

		bufLoc += payloadLen + 1;
	}

}

static void SlippiHandlerThread_Finish(struct ipcmessage *slippi_msg, int retval)
{
	// Prior to setting result, the result of the message is -1
	slippi_msg->result = retval;
	slippi_msg->ioctl.command = 0;
	slippi_msg->ioctl.length_io = 0;
	mqueue_ack(slippi_msg, retval);
}

static u32 SlippiHandlerThread(void *arg) {
	dbgprintf("Slippi Thread ID: %d\r\n", thread_get_id());

	FIL file;
	u32 writtenByteCount = 0;
	struct ipcmessage *slippi_msg;
	while(1)
	{
		mqueue_recv(Slippi_MessageQueue, &slippi_msg, 0);

		if (slippi_msg->ioctl.command == 1) {
			// Create folder if it doesn't exist yet
			f_mkdir_char("usb:/Slippi");

			dbgprintf("Creating File...\r\n");
			char* fileName = generateFileName(true);
			FRESULT fileOpenResult = f_open_char(&file, fileName, FA_CREATE_ALWAYS | FA_WRITE);

			if (fileOpenResult != FR_OK) {
				dbgprintf("Slippi: failed to open file: %s, errno: %d\r\n", fileName, fileOpenResult);
				SlippiHandlerThread_Finish(slippi_msg, fileOpenResult);
				break;
			}

			// dbgprintf("Bytes written: %d/%d...\r\n", wrote, currentBuffer->len);
			writtenByteCount = 0;
			writeHeader(&file);
		}

		UINT wrote;
		f_write(&file, slippi_msg->ioctl.buffer_io, slippi_msg->ioctl.length_io, &wrote);
		f_sync(&file);
		writtenByteCount += wrote;

		if (slippi_msg->ioctl.command == 2) {
			dbgprintf("Completing File...\r\n");
			completeFile(&file, writtenByteCount);
			f_close(&file);
		}

		SlippiHandlerThread_Finish(slippi_msg, 0);
	}
	return 0;
}
