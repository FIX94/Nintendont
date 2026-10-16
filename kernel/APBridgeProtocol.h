#ifndef __APBRIDGE_PROTOCOL_H__
#define __APBRIDGE_PROTOCOL_H__

/* AP Bridge v0.3 wire format. The including file supplies u8/u16/u32. */
#define APBRIDGE_READ_REQUEST_SIZE 12
#define APBRIDGE_READ_MAX_LENGTH 256
#define APBRIDGE_READ_REPLY_SIZE (APBRIDGE_READ_REQUEST_SIZE + APBRIDGE_READ_MAX_LENGTH)
#define APBRIDGE_MEM1_START 0x80000000U
#define APBRIDGE_MEM1_END 0x81800000U /* exclusive */

#define APBRIDGE_READ_BAD_LENGTH 1
#define APBRIDGE_READ_BAD_RANGE 2
#define APBRIDGE_READ_BAD_RESERVED 3

enum APBridgePacketKind {
	APBRIDGE_PACKET_IGNORE,
	APBRIDGE_PACKET_HELLO,
	APBRIDGE_PACKET_READ
};

struct APBridgeReadRequest {
	u32 address;
	u16 length;
	u8 error;
};

static u16 APBridge_ReadBE16(const u8 *bytes)
{
	return ((u16)bytes[0] << 8) | bytes[1];
}

static u32 APBridge_ReadBE32(const u8 *bytes)
{
	return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16)
		| ((u32)bytes[2] << 8) | bytes[3];
}

static void APBridge_WriteBE16(u8 *bytes, u16 value)
{
	bytes[0] = (u8)(value >> 8);
	bytes[1] = (u8)value;
}

static void APBridge_WriteBE32(u8 *bytes, u32 value)
{
	bytes[0] = (u8)(value >> 24);
	bytes[1] = (u8)(value >> 16);
	bytes[2] = (u8)(value >> 8);
	bytes[3] = (u8)value;
}

static enum APBridgePacketKind APBridge_DecodePacket(
	const u8 *packet, u32 size, struct APBridgeReadRequest *request)
{
	u16 reserved;

	if(size == 5 && packet[0] == 'H' && packet[1] == 'E'
		&& packet[2] == 'L' && packet[3] == 'L' && packet[4] == 'O')
		return APBRIDGE_PACKET_HELLO;
	if(size != APBRIDGE_READ_REQUEST_SIZE || packet[0] != 'R'
		|| packet[1] != 'E' || packet[2] != 'A' || packet[3] != 'D')
		return APBRIDGE_PACKET_IGNORE;

	request->address = APBridge_ReadBE32(packet + 4);
	request->length = APBridge_ReadBE16(packet + 8);
	reserved = APBridge_ReadBE16(packet + 10);
	request->error = 0;
	if(request->length == 0 || request->length > APBRIDGE_READ_MAX_LENGTH)
		request->error = APBRIDGE_READ_BAD_LENGTH;
	else if(request->address < APBRIDGE_MEM1_START
		|| request->address >= APBRIDGE_MEM1_END
		|| request->length > APBRIDGE_MEM1_END - request->address)
		request->error = APBRIDGE_READ_BAD_RANGE;
	else if(reserved != 0)
		request->error = APBRIDGE_READ_BAD_RESERVED;
	return APBRIDGE_PACKET_READ;
}

static void APBridge_EncodeReadReply(u8 *reply,
	const struct APBridgeReadRequest *request)
{
	reply[0] = request->error ? 'E' : 'D';
	reply[1] = request->error ? 'R' : 'A';
	reply[2] = request->error ? 'R' : 'T';
	reply[3] = request->error ? '!' : 'A';
	APBridge_WriteBE32(reply + 4, request->address);
	APBridge_WriteBE16(reply + 8, request->length);
	reply[10] = request->error;
	reply[11] = 0;
}

#endif
