#include "SlippiMemory.h"
#include "common.h"
#include "debug.h"

// Memory Settings: Let Slippi use 0x12B80000 - 0x12E80000
#define SlipMemClear() memset32(SlipMem, 0, SlipMemSize)
u8 *SlipMem = (u8 *)0x12B80000;
u32 SlipMemSize = 0x00300000;
volatile u32 OverflowPosition = 0x00300000;
volatile u32 SlipMemCursor = 0x00000000;

u16 getPayloadSize(SlpGameReader *reader, u8 command);
void setPayloadSizes(SlpGameReader *reader, u32 readPos);
void resetMetadata(SlpGameReader *reader);
void updateMetadata(SlpGameReader *reader, u8 *message, u32 messageLength);

/* This should only be dispatched once in kernel/main.c after NCDInit() has
 * actually brought up the networking stack and we have connectivity. */
void SlippiMemoryInit()
{
	SlipMemClear();
}

void SlippiMemoryWrite(const u8 *buf, u32 len)
{
	// Handle overflow logic. Once we are going to overflow, wrap around to start
	// of memory region
	if ((SlipMemCursor + len) > SlipMemSize)
	{
		OverflowPosition = SlipMemCursor;
		SlipMemCursor = 0;
	}

	memcpy(&SlipMem[SlipMemCursor], buf, len);
}

SlpMemError SlippiMemoryRead(SlpGameReader *reader, const u8 *buf, u32 bufLen, u32 readPos)
{
	// Reset previous read result
	reader->lastReadResult.bytesRead = 0;
	reader->lastReadResult.isGameEnd = false;
	reader->lastReadResult.isNewGame = false;

	SlpMemError errCode = SLP_MEM_OK;

	u32 bytesRead = 0;
	while (readPos != SlipMemCursor)
	{
		u8 command = SlipMem[readPos];

		// Special case handling: Unnexpected new game message - shouldn't happen
		if (bytesRead > 0 && command == SLP_CMD_RECEIVE_COMMANDS)
		{
			dbgprintf("WARN: Unnexpected new game message\r\n");
			errCode = SLP_MEM_UNNEX_NG;
			break;
		}

		// Detect new file
		if (command == SLP_CMD_RECEIVE_COMMANDS)
		{
			reader->lastReadResult.isNewGame = true;
			resetMetadata(reader);
			setPayloadSizes(reader, readPos);
		}

		u16 payloadSize = getPayloadSize(reader, command);

		// Special case handling: Payload size not found for command - shouldn't happen
		if (payloadSize == 0)
		{
			dbgprintf("WARN: Payload size not detected\r\n");
			errCode = SLP_PL_MISSING;
			break;
		}

		u16 bytesToRead = payloadSize + 1;

		// Special case handling: buffer is full
		if ((bytesRead + bytesToRead) > bufLen)
			break;

		// Copy payload data into buffer. We don't need to sync_after_write because
		// the memory will be read from the ARM side
		memcpy(&buf[bytesRead], &SlipMem[readPos], bytesToRead);

		// Handle updating metadata
		updateMetadata(reader, &buf[bytesRead], bytesToRead);

		// Increment both read positions
		readPos += bytesToRead;
		bytesRead += bytesToRead;

		// Special case handling: game end message processed
		if (command == SLP_CMD_RECEIVE_GAME_END)
		{
			reader->lastReadResult.isGameEnd = true;
			break;
		}

		// Handle overflow. If readPos is at OverflowPosition, loop to start
		if (readPos >= OverflowPosition)
			readPos = 0;
	}

	reader->lastReadPos = readPos;
	reader->lastReadResult.bytesRead = bytesRead;

	return errCode;
}

void resetMetadata(SlpGameReader *reader)
{
	// TODO: Test this
	SlpMetadata freshMetadata;
	reader->metadata = freshMetadata;
}

void updateMetadata(SlpGameReader *reader, u8 *message, u32 messageLength)
{
	u8 command = message[0];
	if (messageLength <= 0 || command != SLP_CMD_RECEIVE_POST_FRAME_UPDATE)
	{
		// Only need to update if this is a post frame update
		return;
	}

	// Keep track of last frame
	reader->metadata.lastFrame = message[1] << 24 | message[2] << 16 | message[3] << 8 | message[4];

	// TODO: Add character usage
	// Keep track of character usage
	// u8 playerIndex = payload[5];
	// u8 internalCharacterId = payload[7];
	// if (!characterUsage.count(playerIndex) || !characterUsage[playerIndex].count(internalCharacterId)) {
	// 	characterUsage[playerIndex][internalCharacterId] = 0;
	// }
	// characterUsage[playerIndex][internalCharacterId] += 1;
}

void setPayloadSizes(SlpGameReader *reader, u32 readPos)
{
	// Clear previous payloadSizes
	memset(reader->payloadSizes, 0, PAYLOAD_SIZES_BUFFER_SIZE);

	u8 *payload = &SlipMem[readPos + 1];
	u8 length = payload[0];

	int i = 1;
	while (i < length)
	{
		// Go through the receive commands payload and set up other commands
		u8 commandByte = payload[i];
		u16 commandPayloadSize = payload[i + 1] << 8 | payload[i + 2];
		reader->payloadSizes[commandByte - SLP_CMD_RECEIVE_COMMANDS] = commandPayloadSize;

		// dbgprintf("Index: 0x%02X, Size: 0x%02X\r\n", commandByte - CMD_RECEIVE_COMMANDS, commandPayloadSize);

		i += 3;
	}
}

u16 getPayloadSize(SlpGameReader *reader, u8 command)
{
	int payloadSizesIndex = command - SLP_CMD_RECEIVE_COMMANDS;
	if (payloadSizesIndex >= PAYLOAD_SIZES_BUFFER_SIZE || payloadSizesIndex < 0)
	{
		return 0;
	}

	return reader->payloadSizes[payloadSizesIndex];
}
