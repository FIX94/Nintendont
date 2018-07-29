#include "Slippi.h"
#include "time.h"
#include "alloc.h"
#include "debug.h"
#include "string.h"
#include "ff_utf8.h"
#include "DI.h"

#define PAYLOAD_BUFFER_SIZE 0x200 // Current largest payload is 0x15D in length
#define FRAME_PAYLOAD_BUFFER_SIZE 0x80
#define WRITE_BUFFER_LENGTH 0x1000
#define PAYLOAD_SIZES_BUFFER_SIZE 20
#define FOOTER_BUFFER_LENGTH 200

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
static u32 Slippi_Thread = 0;
extern char __slippi_stack_addr, __slippi_stack_size;

typedef struct BufferAccess {
	bool isInUse; // Is set to true when main thread starts writting to this buf
	bool isFilled; // Is set to true when main thread finished writting to this buf
	u8 fileAction; // 0 = no action, 1 = create file, 2 = close file
	u32 len; // Length of the data in the buffer
	u8 buffer[WRITE_BUFFER_LENGTH]; // Data to write
} bufferAccess;

#define BUFFER_ACCESS_COUNT 10
static bufferAccess accessManager[BUFFER_ACCESS_COUNT];
static u32 writeBufferIndex = 0;
static u32 processBufferIndex = 0;

// File writing stuff
static u32 fileIndex = 1;

// .slp File creation stuff
static u32 writtenByteCount = 0;

// vars for metadata generation
// time_t gameStartTime;
s32 lastFrame;

// Payload
static u32 m_payload_loc = 0;
static u8 m_payload_type = CMD_UNKNOWN;
static u8 m_payload[PAYLOAD_BUFFER_SIZE];

// Payload Sizes
static u16 payloadSizes[PAYLOAD_SIZES_BUFFER_SIZE];

void SlippiInit()
{
	// Set the commands payload to start at length 1. The reason for this
	// is that the game will pass in all the command sizes but if
	// it starts at 0 then the command is ignored and nothing ever happens
	payloadSizes[0] = 1;

	Slippi_Thread = do_thread_create(SlippiHandlerThread, ((u32*)&__slippi_stack_addr), ((u32)(&__slippi_stack_size)), 0x78);
	thread_continue(Slippi_Thread);
}

void SlippiShutdown()
{
	thread_cancel(Slippi_Thread, 0);
}

char *generateFileName(bool isNewFile)
{
	// // Add game start time
	// u8 dateTimeStrLength = sizeof "20171015T095717";
	// char *dateTimeBuf = (char *)malloc(dateTimeStrLength);
	// strftime(&dateTimeBuf[0], dateTimeStrLength, "%Y%m%dT%H%M%S", localtime(&gameStartTime));

	// std::string str(&dateTimeBuf[0]);
	// return StringFromFormat("Slippi/Game_%s.slp", str.c_str());

	static char pathStr[30];

	_sprintf(&pathStr[0], "usb:/Slippi/Game-%d.slp", fileIndex);

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

		// if (debugCounter <= debugMax)
		// 	dbgprintf("Index: %02X, Size: %02X\r\n", commandByte - CMD_RECEIVE_COMMANDS, commandPayloadSize);

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
}

void completeFile(FIL *file) {
	u8 footer[FOOTER_BUFFER_LENGTH];
	u32 writePos = 0;

	// Write opener
	u8 footerOpener[] = { 'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{' };
	u8 writeLen = sizeof(footerOpener);
	memcpy(&footer[writePos], footerOpener, writeLen);
	writePos += writeLen;

	// Write startAt
	char timeStr[] = "2011-10-08T07:07:09Z"; // TODO: get actual time str
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

	f_lseek(file, 11);
	f_write(file, &writtenByteCount, 4, &wrote);
}

void processPayload(u8 *payload, u32 length, u8 fileOption)
{
	bufferAccess *currentBuffer = &accessManager[writeBufferIndex];
	if (currentBuffer->isFilled) {
		dbgprintf("ERROR: Buffers not processed fast enough\r\n");
	}

	// Mark this buffer as in use. Is this needed?
	currentBuffer->isInUse = true;
	if (fileOption > 0) {
		currentBuffer->fileAction = fileOption;
	}

	// Copy data to write buffer
	u32 writePos = currentBuffer->len;
	memcpy(&currentBuffer->buffer[writePos], payload, length);
	currentBuffer->len += length;
	
	updateMetadataFields(payload, length);

	// If write buffer is not full yet, don't do anything else
	bool isBufferFull = currentBuffer->len >= WRITE_BUFFER_LENGTH - FRAME_PAYLOAD_BUFFER_SIZE;
	bool isGameComplete = fileOption == 2;
	bool isGameStart = payload[0] == CMD_RECEIVE_GAME_INFO;
	bool shouldWrite = isBufferFull || isGameComplete || isGameStart;
	if (!shouldWrite) {
		return;
	}

	currentBuffer->isFilled = true;
	writeBufferIndex = (writeBufferIndex + 1) % BUFFER_ACCESS_COUNT;
}

void SlippiImmWrite(u32 data, u32 size)
{
	bool lookingForMessage = m_payload_type == CMD_UNKNOWN;
	if (lookingForMessage)
	{
		// If the size is not one, this can't be the start of a command
		if (size != 1)
		{
			return;
		}

		m_payload_type = data >> 24;

		// Attempt to get payload size for this command. If not found, don't do anything
		// Obviously as written, commands with payloads of size zero will not work, there
		// are currently no such commands atm
		u16 payloadSize = getPayloadSize(m_payload_type);
		if (payloadSize == 0)
		{
			m_payload_type = CMD_UNKNOWN;
			return;
		}
	}

	// Add new data to payload
	int i = 0;
	while (i < size)
	{
		int shiftAmount = 8 * (3 - i);
		u8 byte = 0xFF & (data >> shiftAmount);
		m_payload[m_payload_loc] = byte;

		m_payload_loc += 1;

		i++;
	}

	// This section deals with saying we are done handling the payload
	// add one because we count the command as part of the total size
	u16 payloadSize = getPayloadSize(m_payload_type);
	if (m_payload_type == CMD_RECEIVE_COMMANDS && m_payload_loc > 1)
	{
		// the receive commands command tells us exactly how long it is
		// this is to make adding new commands easier
		payloadSize = m_payload[1];
	}

	if (m_payload_loc >= payloadSize + 1)
	{
		// Handle payloads
		switch (m_payload_type)
		{
		case CMD_RECEIVE_COMMANDS:
			// time(&gameStartTime); // Store game start time
			configureCommands(&m_payload[1], m_payload_loc - 1);
			processPayload(&m_payload[0], m_payload_loc, 1);
			break;
		case CMD_RECEIVE_GAME_END:
			processPayload(&m_payload[0], m_payload_loc, 2);
			break;
		default:
			processPayload(&m_payload[0], m_payload_loc, 0);
			break;
		}

		// reset payload loc and type so we look for next command
		m_payload_loc = 0;
		m_payload_type = CMD_UNKNOWN;
	}
}

void SlippiDmaWrite(const void *buf, u32 len) {
	sync_before_read((void*)buf, len);
	memcpy(&m_payload, buf, len);
	sync_after_write(&m_payload, len);
	
	u8 command = m_payload[0];
	
	// Handle payloads
	switch (command)
	{
	case CMD_RECEIVE_COMMANDS:
		// time(&gameStartTime); // Store game start time
		configureCommands(&(m_payload[1]), len - 1);
		processPayload(&(m_payload[0]), len, 1);
		break;
	case CMD_RECEIVE_GAME_END:
		processPayload(&(m_payload[0]), len, 2);
		break;
	default:
		processPayload(&(m_payload[0]), len, 0);
		break;
	}
}

void handleCurrentBuffer() {
	bufferAccess *currentBuffer = &accessManager[processBufferIndex];
	if (!currentBuffer->isFilled) {
		return;
	}

	dbgprintf("Found a filled buffer. Len: %d | Command: %02X\r\n", currentBuffer->len, currentBuffer->buffer[0]);

	// DIFinishAsync(); //DONT ever try todo file i/o async

	static FIL file;
	if (currentBuffer->fileAction == 1) {
		dbgprintf("Creating File...\r\n");
		char* fileName = generateFileName(true);
		FRESULT fileOpenResult = f_open_char(&file, fileName, FA_CREATE_ALWAYS | FA_WRITE);

		if (fileOpenResult != FR_OK) {
			dbgprintf("ERROR: Failed to open file: %d...\r\n", fileOpenResult);
			mdelay(100);
			return;
		}

		writtenByteCount = 0;
		writeHeader(&file);
	}

	u32 wrote;
	f_write(&file, currentBuffer->buffer, currentBuffer->len, &wrote);
	writtenByteCount += currentBuffer->len;

	f_sync(&file);

	if (currentBuffer->fileAction == 2) {
		dbgprintf("Completing File...\r\n");
		completeFile(&file);
		f_sync(&file);
		f_close(&file);
	}

	dbgprintf("Bytes written: %d/%d...\r\n", wrote, currentBuffer->len);

	currentBuffer->len = 0;
	currentBuffer->fileAction = 0;
	currentBuffer->isInUse = false;
	currentBuffer->isFilled = false;
	
	processBufferIndex = (processBufferIndex + 1) % BUFFER_ACCESS_COUNT;
}

u32 SlippiHandlerThread(void *arg) {
	dbgprintf("Slippi Thread ID: %d\r\n", thread_get_id());

	while(1)
	{
		mdelay(10);
		handleCurrentBuffer();
	}

	return 0;
}
