#include "Slippi.h"
#include "time.h"
#include "malloc.h"
#include "debug.h"
#include "string.h"
#include "ff_utf8.h"

#define PAYLOAD_BUFFER_SIZE 0x200 // Current largest payload is 0x15D in length
#define PAYLOAD_SIZES_BUFFER_SIZE 10

// Debugging
static u32 debugCounter = 0;
static u32 debugMax = 50;

enum
{
	CMD_UNKNOWN = 0x0,
	CMD_RECEIVE_COMMANDS = 0x35,
	CMD_RECEIVE_GAME_INFO = 0x36,
	CMD_RECEIVE_PRE_FRAME_UPDATE = 0x37,
	CMD_RECEIVE_POST_FRAME_UPDATE = 0x38,
	CMD_RECEIVE_GAME_END = 0x39,
};

static FIL m_file;
static bool isFileOpen = false;

// .slp File creation stuff
// static u32 writtenByteCount = 0;

// vars for metadata generation
// time_t gameStartTime;
// u32 lastFrame;

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
}

char *generateFileName()
{
	// // Add game start time
	// u8 dateTimeStrLength = sizeof "20171015T095717";
	// char *dateTimeBuf = (char *)malloc(dateTimeStrLength);
	// strftime(&dateTimeBuf[0], dateTimeStrLength, "%Y%m%dT%H%M%S", localtime(&gameStartTime));

	// std::string str(&dateTimeBuf[0]);
	// return StringFromFormat("Slippi/Game_%s.slp", str.c_str());

	static char pathStr[30];

	_sprintf(&pathStr[0], "/Slippi/Game.slp");

	return pathStr;
}

void closeSlpFile()
{
	if (!isFileOpen)
	{
		// If we have no file or payload is not game end, do nothing
		return;
	}

	// If this is the end of the game end payload, reset the file so that we create a new one
	f_close(&m_file);
	isFileOpen = false;
}

void createSlpFile()
{
	if (isFileOpen)
	{
		// If there's already a file open, close that one
		closeSlpFile();
	}

	f_mkdir_char("/Slippi");

	char *filepath = generateFileName();
	f_open_char(&m_file, filepath, FA_CREATE_ALWAYS | FA_WRITE);
	free(filepath);

	isFileOpen = true;
}

void writeSlpFile(u8 *data, u32 length)
{
	if (!isFileOpen)
	{
		return;
	}

	u32 bw;
	f_write(&m_file, data, length, &bw);

	if (bw != length)
	{
		// Disk is full, do something?
	}
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

// void updateMetadataFields(u8* payload, u32 length) {
// 	if (length <= 0 || payload[0] != CMD_RECEIVE_POST_FRAME_UPDATE) {
// 		// Only need to update if this is a post frame update
// 		return;
// 	}

// 	// Keep track of last frame
// 	lastFrame = payload[1] << 24 | payload[2] << 16 | payload[3] << 8 | payload[4];

// 	// TODO: Add character usage
// 	// Keep track of character usage
// 	// u8 playerIndex = payload[5];
// 	// u8 internalCharacterId = payload[7];
// 	// if (!characterUsage.count(playerIndex) || !characterUsage[playerIndex].count(internalCharacterId)) {
// 	// 	characterUsage[playerIndex][internalCharacterId] = 0;
// 	// }
// 	// characterUsage[playerIndex][internalCharacterId] += 1;
// }

// u8* generateMetadata() {
// 	u8* output = malloc()
// 	std::vector<u8> metadata(
// 		{ 'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{' }
// 	);

// 	// TODO: Abstract out UBJSON functions to make this cleaner

// 	// Add game start time
// 	uint8_t dateTimeStrLength = sizeof "2011-10-08T07:07:09Z";
// 	char* dateTimeBuf = malloc(dateTimeStrLength);
// 	strftime(&dateTimeBuf[0], dateTimeStrLength, "%FT%TZ", gmtime(&gameStartTime));
// 	dateTimeBuf.pop_back(); // Removes the \0 from the back of string
// 	metadata.insert(metadata.end(), {
// 		'U', 7, 's', 't', 'a', 'r', 't', 'A', 't', 'S', 'U', (uint8_t)dateTimeBuf.size()
// 	});
// 	metadata.insert(metadata.end(), dateTimeBuf.begin(), dateTimeBuf.end());

// 	// Add game duration
// 	std::vector<u8> lastFrameToWrite = int32ToVector(lastFrame);
// 	metadata.insert(metadata.end(), {
// 		'U', 9, 'l', 'a', 's', 't', 'F', 'r', 'a', 'm', 'e', 'l'
// 	});
// 	metadata.insert(metadata.end(), lastFrameToWrite.begin(), lastFrameToWrite.end());

// 	// Add players elements to metadata, one per player index
// 	// metadata.insert(metadata.end(), {
// 	// 	'U', 7, 'p', 'l', 'a', 'y', 'e', 'r', 's', '{'
// 	// });
// 	// for (auto it = characterUsage.begin(); it != characterUsage.end(); ++it) {
// 	// 	metadata.push_back('U');
// 	// 	std::string playerIndexStr = std::to_string(it->first);
// 	// 	metadata.push_back((u8)playerIndexStr.length());
// 	// 	metadata.insert(metadata.end(), playerIndexStr.begin(), playerIndexStr.end());
// 	// 	metadata.push_back('{');

// 	// 	// Add character element for this player
// 	// 	metadata.insert(metadata.end(), {
// 	// 		'U', 10, 'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', 's', '{'
// 	// 	});
// 	// 	for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
// 	// 		metadata.push_back('U');
// 	// 		std::string internalCharIdStr = std::to_string(it2->first);
// 	// 		metadata.push_back((u8)internalCharIdStr.length());
// 	// 		metadata.insert(metadata.end(), internalCharIdStr.begin(), internalCharIdStr.end());

// 	// 		metadata.push_back('l');
// 	// 		std::vector<u8> frameCount = uint32ToVector(it2->second);
// 	// 		metadata.insert(metadata.end(), frameCount.begin(), frameCount.end());
// 	// 	}
// 	// 	metadata.push_back('}'); // close characters

// 	// 	metadata.push_back('}'); // close player
// 	// }
// 	// metadata.push_back('}');

// 	// Indicate this was played on dolphin
// 	metadata.insert(metadata.end(), {
// 		'U', 8, 'p', 'l', 'a', 'y', 'e', 'd', 'O', 'n', 'S', 'U',
// 		10, 'n', 'i', 'n', 't', 'e', 'n', 'd', 'o', 'n', 't'
// 	});

// 	// TODO: Add player names

// 	metadata.push_back('}');
// 	return metadata;
// }

void processPayload(u8 *payload, u32 length, u8 fileOption)
{
	// DEBUG MESSAGES RECEIVED
	// char *toWrite = malloc((2 * length) + 1);

	// int i = 0;
	// while (i < length)
	// {
	// 	_sprintf(&toWrite[i * 2], "%02X", payload[i]);
	// 	i++;
	// }

	// toWrite[(2 * length) + 1] = '\0';

	// dbgprintf("%s\r\n", toWrite);

	// free(toWrite);

	// WRITE FILE TEST
	if (fileOption == 1) {
		createSlpFile();
	}

	writeSlpFile(payload, length);

	if (fileOption == 2) {
		closeSlpFile();
	}

	// std::vector<u8> dataToWrite;
	// if (fileOption == 1) {
	// 	// If the game sends over option 1 that means a file should be created
	// 	createNewFile();

	// 	// Start ubjson file and prepare the "raw" element that game
	// 	// data output will be dumped into. The size of the raw output will
	// 	// be initialized to 0 until all of the data has been received
	// 	std::vector<u8> headerBytes(
	// 		{ '{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0 }
	// 	);
	// 	dataToWrite.insert(dataToWrite.end(), headerBytes.begin(), headerBytes.end());

	// 	// Used to keep track of how many bytes have been written to the file
	// 	writtenByteCount = 0;

	// 	// Used to track character usage (sheik/zelda)
	// 	characterUsage.clear();

	// 	// Reset lastFrame
	// 	lastFrame = Slippi::GAME_FIRST_FRAME;
	// }

	// // If no file, do nothing
	// if (!m_file) {
	// 	return;
	// }

	// // Update fields relevant to generating metadata at the end
	// updateMetadataFields(payload, length);

	// // Add the payload to data to write
	// dataToWrite.insert(dataToWrite.end(), payload, payload + length);
	// writtenByteCount += length;

	// // If we are going to close the file, generate data to complete the UBJSON file
	// if (fileOption == 2) {
	// 	// This option indicates we are done sending over body
	// 	std::vector<u8> closingBytes = generateMetadata();
	// 	closingBytes.push_back('}');
	// 	dataToWrite.insert(dataToWrite.end(), closingBytes.begin(), closingBytes.end());
	// }

	// // Write data to file
	// bool result = m_file.WriteBytes(&dataToWrite[0], dataToWrite.size());
	// if (!result) {
	// 	ERROR_LOG(EXPANSIONINTERFACE, "Failed to write data to file.");
	// }

	// // If file should be closed, close it
	// if (fileOption == 2) {
	// 	// Write the number of bytes for the raw output
	// 	std::vector<u8> sizeBytes = uint32ToVector(writtenByteCount);
	// 	m_file.Seek(11, 0);
	// 	m_file.WriteBytes(&sizeBytes[0], sizeBytes.size());

	// 	// Close file
	// 	closeFile();
	// }
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
