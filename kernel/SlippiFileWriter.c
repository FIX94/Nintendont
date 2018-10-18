#include "SlippiFileWriter.h"
#include "SlippiMemory.h"
#include "alloc.h"
#include "debug.h"
#include "string.h"
#include "ff_utf8.h"

#include "SlippiDebug.h"

// Game can transfer at most 784 bytes / frame
// That means 4704 bytes every 100 ms. Let's aim to handle
// double that, making our read buffer 10000 bytes
#define READ_BUF_SIZE 10000
#define THREAD_CYCLE_TIME_MS 100

#define FOOTER_BUFFER_LENGTH 200

static u32 SlippiHandlerThread(void *arg);

// Thread stuff
static u32 Slippi_Thread;
extern char __slippi_stack_addr, __slippi_stack_size;

// File writing stuff
static u32 fileIndex = 1;

// File object
FIL currentFile;

// vars for metadata generation
u32 gameStartTime;

void SlippiFileWriterInit()
{
	Slippi_Thread = do_thread_create(
		SlippiHandlerThread,
		((u32 *)&__slippi_stack_addr),
		((u32)(&__slippi_stack_size)),
		0x78);
	thread_continue(Slippi_Thread);
}

void SlippiFileWriterShutdown()
{
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
		tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	if (isNewFile)
	{
		fileIndex += 1;
	}

	return pathStr;
}

void writeHeader(FIL *file)
{
	u8 header[] = {'{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0};

	u32 wrote;
	f_write(file, header, sizeof(header), &wrote);
	f_sync(file);
}

void completeFile(FIL *file, SlpGameReader *reader, u32 writtenByteCount)
{
	u8 footer[FOOTER_BUFFER_LENGTH];
	u32 writePos = 0;

	// Write opener
	u8 footerOpener[] = {'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{'};
	u8 writeLen = sizeof(footerOpener);
	memcpy(&footer[writePos], footerOpener, writeLen);
	writePos += writeLen;

	// Write startAt
	// TODO: Figure out how to specify time zone
	char timeStr[] = "2011-10-08T07:07:09";
	struct tm *tmp = gmtime(&gameStartTime);
	_sprintf(
		&timeStr[0], "%04d-%02d-%02dT%02d:%02d:%02d", tmp->tm_year + 1900,
		tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	u8 startAtOpener[] = {'U', 7, 's', 't', 'a', 'r', 't', 'A', 't', 'S', 'U', (u8)sizeof(timeStr)};
	writeLen = sizeof(startAtOpener);
	memcpy(&footer[writePos], startAtOpener, writeLen);
	writePos += writeLen;
	writeLen = sizeof(timeStr);
	memcpy(&footer[writePos], timeStr, writeLen);
	writePos += writeLen;

	// Write lastFrame
	u8 lastFrameOpener[] = {'U', 9, 'l', 'a', 's', 't', 'F', 'r', 'a', 'm', 'e', 'l'};
	writeLen = sizeof(lastFrameOpener);
	memcpy(&footer[writePos], lastFrameOpener, writeLen);
	writePos += writeLen;
	memcpy(&footer[writePos], &reader->metadata.lastFrame, 4);
	writePos += 4;

	// Write closing
	u8 closing[] = {
		'U', 7, 'p', 'l', 'a', 'y', 'e', 'r', 's', '{', '}',
		'U', 8, 'p', 'l', 'a', 'y', 'e', 'd', 'O', 'n', 'S', 'U',
		10, 'n', 'i', 'n', 't', 'e', 'n', 'd', 'o', 'n', 't',
		'}', '}'};
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

static u32 SlippiHandlerThread(void *arg)
{
	dbgprintf("Slippi Thread ID: %d\r\n", thread_get_id());

	static SlpGameReader reader;
	static u8 readBuf[READ_BUF_SIZE];
	static u64 memReadPos = 0;

	u32 writtenByteCount = 0;
	while (1)
	{
		// Cycle time, look at const definition for more info
		mdelay(THREAD_CYCLE_TIME_MS);

		// TODO: Ensure connection to USB is correct

		// Read from memory and write to file
		SlpMemError err = SlippiMemoryRead(&reader, readBuf, READ_BUF_SIZE, memReadPos);
		if (err)
		{
			if (err == SLP_READ_OVERFLOW)
				memReadPos = SlippiRestoreReadPos();
				
			mdelay(1000);
			
			// For specific errors, bytes will still be read. Not continueing to deal with those
		}

		if (reader.lastReadResult.isNewGame)
		{
			// Create folder if it doesn't exist yet
			f_mkdir_char("usb:/Slippi");

			gameStartTime = GetCurrentTime();

			dbgprintf("Creating File...\r\n");
			char *fileName = generateFileName(true);
			// Need to open with FA_READ if network thread is going to share &currentFile
			FRESULT fileOpenResult = f_open_char(&currentFile, fileName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
			if (fileOpenResult != FR_OK)
			{
				dbgprintf("Slippi: failed to open file: %s, errno: %d\r\n", fileName, fileOpenResult);
				mdelay(1000);
				continue;
			}

			// dbgprintf("Bytes written: %d/%d...\r\n", wrote, currentBuffer->len);
			writtenByteCount = 0;
			writeHeader(&currentFile);
		}

		if (reader.lastReadResult.bytesRead == 0)
			continue;

		// dbgprintf("Bytes read: %d\r\n", reader.lastReadResult.bytesRead);

		UINT wrote;
		f_write(&currentFile, readBuf, reader.lastReadResult.bytesRead, &wrote);
		f_sync(&currentFile);

		if (wrote == 0)
			continue;

		// Only increment mem read position when the data is correctly written
		memReadPos += wrote;
		writtenByteCount += wrote;

		if (reader.lastReadResult.isGameEnd)
		{
			dbgprintf("Completing File...\r\n");
			completeFile(&currentFile, &reader, writtenByteCount);
			f_close(&currentFile);
		}
	}

	return 0;
}
