#include "SlippiMemory.h"
#include "common.h"
#include "debug.h"

// Memory Settings: Let Slippi use 0x12B80000 - 0x12E80000
u8 *SlipMem = (u8 *)0x12B80000;
u32 SlipMemSize = 0x00300000;
volatile u32 OverflowPosition = 0x00300000;
volatile u32 SlipMemCursor = 0x00000000;
void SlipMemInit(void) { memset32(SlipMem, 0, SlipMemSize); }

u16 getPayloadSize(SlpGame *game, u8 command);

/* This should only be dispatched once in kernel/main.c after NCDInit() has
 * actually brought up the networking stack and we have connectivity. */
void SlippiMemoryInit()
{
	SlipMemInit();
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

SlpMemError SlippiMemoryRead(SlpGame *game, const u8 *buf, u32 bufLen, u32 readPos)
{
	// Reset previous read result
	game->lastReadResult.bytesRead = 0;
	game->lastReadResult.isGameEnd = false;
	game->lastReadResult.isNewGame = false;

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
			game->lastReadResult.isNewGame = true;
			setPayloadSizes(game, readPos);
		}

		u16 payloadSize = getPayloadSize(game, command);

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

		// Copy payload data into buffer
		memcpy(&buf[bytesRead], &SlipMem[readPos], bytesToRead);

		// Increment both read positions
		readPos += bytesToRead;
		bytesRead += bytesToRead;

		// Special case handling: game end message processed
		if (command == SLP_CMD_RECEIVE_GAME_END)
		{
			game->lastReadResult.isGameEnd = true;
			break;
		}

		// Handle overflow. If readPos is at OverflowPosition, loop to start
		if (readPos >= OverflowPosition)
			readPos = 0;
	}

	game->lastReadResult.bytesRead = bytesRead;

	return errCode;
}

void setPayloadSizes(SlpGame *game, u32 readPos)
{
	// TODO: Clear previous payloadSizes

	u8 *payload = &SlipMem[readPos + 1];
	u8 length = payload[0];

	int i = 1;
	while (i < length)
	{
		// Go through the receive commands payload and set up other commands
		u8 commandByte = payload[i];
		u16 commandPayloadSize = payload[i + 1] << 8 | payload[i + 2];
		game->payloadSizes[commandByte - SLP_CMD_RECEIVE_COMMANDS] = commandPayloadSize;

		// dbgprintf("Index: 0x%02X, Size: 0x%02X\r\n", commandByte - CMD_RECEIVE_COMMANDS, commandPayloadSize);

		i += 3;
	}
}

u16 getPayloadSize(SlpGame *game, u8 command)
{
	int payloadSizesIndex = command - SLP_CMD_RECEIVE_COMMANDS;
	if (payloadSizesIndex >= PAYLOAD_SIZES_BUFFER_SIZE || payloadSizesIndex < 0)
	{
		return 0;
	}

	return game->payloadSizes[payloadSizesIndex];
}
