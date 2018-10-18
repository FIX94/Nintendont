#ifndef __SLIPPI_MEMORY_H__
#define __SLIPPI_MEMORY_H__

#include "global.h"

enum
{
	SLP_CMD_UNKNOWN = 0x0,
	SLP_CMD_RECEIVE_COMMANDS = 0x35,
	SLP_CMD_RECEIVE_GAME_INFO = 0x36,
	SLP_CMD_RECEIVE_PRE_FRAME_UPDATE = 0x37,
	SLP_CMD_RECEIVE_POST_FRAME_UPDATE = 0x38,
	SLP_CMD_RECEIVE_GAME_END = 0x39,
};

typedef enum
{
	SLP_MEM_OK = 0,    /* (0) Succeeded */
	SLP_MEM_UNNEX_NG,  /* (1) Unnexpected new game */
	SLP_PL_MISSING,    /* (2) Expected a command byte with a payload size, but got something else */
	SLP_READ_OVERFLOW, /* (3) Overflow read, the read position requested is too far behind */
} SlpMemError;

typedef struct SlpMetadata
{
	s32 lastFrame;
} SlpMetadata;

typedef struct SlpReadResult
{
	bool isNewGame;
	bool isGameEnd;
	u32 bytesRead;
} SlpReadResult;

#define PAYLOAD_SIZES_BUFFER_SIZE 10

typedef struct SlpGameReader
{
	u16 payloadSizes[PAYLOAD_SIZES_BUFFER_SIZE];
	u64 lastReadPos;
	SlpMetadata metadata;
	SlpReadResult lastReadResult;
} SlpGameReader;

void SlippiMemoryInit();

void SlippiMemoryWrite(const u8 *buf, u32 len);

// The same game object should be passed to this function every call. This function
// will always read full payloads into the buffer, partial payloads will not be loaded
SlpMemError SlippiMemoryRead(SlpGameReader *reader, u8 *buf, u32 bufLen, u64 readPos);

// Will restore a read position 
u64 SlippiRestoreReadPos();

#endif
