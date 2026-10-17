#ifndef __APBRIDGE_PROTOCOL_H__
#define __APBRIDGE_PROTOCOL_H__

/* AP Bridge memory wire format. The including file supplies u8/u16/u32. */
#define APBRIDGE_READ_REQUEST_SIZE 12
#define APBRIDGE_READ_MAX_LENGTH 256
#define APBRIDGE_READ_REPLY_SIZE (APBRIDGE_READ_REQUEST_SIZE + APBRIDGE_READ_MAX_LENGTH)
#define APBRIDGE_WRITE_REQUEST_SIZE 12
#define APBRIDGE_WRITE_MAX_LENGTH APBRIDGE_READ_MAX_LENGTH
#define APBRIDGE_WRITE_MAX_SIZE (APBRIDGE_WRITE_REQUEST_SIZE + APBRIDGE_WRITE_MAX_LENGTH)
#define APBRIDGE_TX_REQUEST_SIZE 20
#define APBRIDGE_TX_MAX_SIZE (APBRIDGE_TX_REQUEST_SIZE + APBRIDGE_WRITE_MAX_LENGTH)
/* Keep one extra byte so an oversized datagram cannot look like a valid maximum. */
#define APBRIDGE_PACKET_BUFFER_SIZE (APBRIDGE_TX_MAX_SIZE + 1)
#define APBRIDGE_TX_CACHE_ENTRIES 16
#define APBRIDGE_MEM1_START 0x80000000U
#define APBRIDGE_MEM1_END 0x81800000U /* exclusive */

#define APBRIDGE_READ_BAD_LENGTH 1
#define APBRIDGE_READ_BAD_RANGE 2
#define APBRIDGE_READ_BAD_RESERVED 3
#define APBRIDGE_WRITE_BAD_PAYLOAD_SIZE 4
#define APBRIDGE_TX_BAD_ID 5
#define APBRIDGE_TX_CONFLICT 6

enum APBridgePacketKind {
	APBRIDGE_PACKET_IGNORE,
	APBRIDGE_PACKET_HELLO,
	APBRIDGE_PACKET_READ,
	APBRIDGE_PACKET_WRITE,
	APBRIDGE_PACKET_WRITE_TX
};

struct APBridgeMemoryRequest {
	u32 address;
	u16 length;
	u8 error;
	u32 txHigh;
	u32 txLow;
};

/* A zero ID marks an unused slot. This is a bounded recent cache: callers
 * must not intentionally reuse IDs after eviction.
 */
struct APBridgeTxEntry {
	u32 txHigh;
	u32 txLow;
	u32 address;
	u32 length;
	u8 payload[APBRIDGE_WRITE_MAX_LENGTH];
};

struct APBridgeTxCache {
	struct APBridgeTxEntry entries[APBRIDGE_TX_CACHE_ENTRIES];
	u32 next;
};

enum APBridgeTxLookupResult {
	APBRIDGE_TX_NEW,
	APBRIDGE_TX_DUPLICATE,
	APBRIDGE_TX_ID_CONFLICT
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

static u8 APBridge_ValidateMemoryRequest(
	const struct APBridgeMemoryRequest *request, u16 reserved)
{
	if(request->length == 0 || request->length > APBRIDGE_READ_MAX_LENGTH)
		return APBRIDGE_READ_BAD_LENGTH;
	if(request->address < APBRIDGE_MEM1_START
		|| request->address >= APBRIDGE_MEM1_END
		|| request->length > APBRIDGE_MEM1_END - request->address)
		return APBRIDGE_READ_BAD_RANGE;
	if(reserved != 0)
		return APBRIDGE_READ_BAD_RESERVED;
	return 0;
}

static enum APBridgePacketKind APBridge_DecodePacket(
	const u8 *packet, u32 size, struct APBridgeMemoryRequest *request)
{
	u16 reserved;
	enum APBridgePacketKind kind;

	if(size == 5 && packet[0] == 'H' && packet[1] == 'E'
		&& packet[2] == 'L' && packet[3] == 'L' && packet[4] == 'O')
		return APBRIDGE_PACKET_HELLO;
	if(size < APBRIDGE_WRITE_REQUEST_SIZE)
		return APBRIDGE_PACKET_IGNORE;
	if(packet[0] == 'R' && packet[1] == 'E'
		&& packet[2] == 'A' && packet[3] == 'D')
	{
		if(size != APBRIDGE_READ_REQUEST_SIZE)
			return APBRIDGE_PACKET_IGNORE;
		kind = APBRIDGE_PACKET_READ;
	}
	else if(packet[0] == 'W' && packet[1] == 'R'
		&& packet[2] == 'I' && packet[3] == 'T')
		kind = APBRIDGE_PACKET_WRITE;
	else if(packet[0] == 'W' && packet[1] == 'R'
		&& packet[2] == 'T' && packet[3] == 'X')
	{
		if(size < APBRIDGE_TX_REQUEST_SIZE)
			return APBRIDGE_PACKET_IGNORE;
		kind = APBRIDGE_PACKET_WRITE_TX;
	}
	else
		return APBRIDGE_PACKET_IGNORE;

	request->address = APBridge_ReadBE32(packet + 4);
	request->length = APBridge_ReadBE16(packet + 8);
	reserved = APBridge_ReadBE16(packet + 10);
	request->txHigh = 0;
	request->txLow = 0;
	if(kind == APBRIDGE_PACKET_WRITE_TX)
	{
		request->txHigh = APBridge_ReadBE32(packet + 12);
		request->txLow = APBridge_ReadBE32(packet + 16);
	}
	request->error = APBridge_ValidateMemoryRequest(request, reserved);
	if(kind == APBRIDGE_PACKET_WRITE && request->error == 0
		&& size != (u32)(APBRIDGE_WRITE_REQUEST_SIZE + request->length))
		request->error = APBRIDGE_WRITE_BAD_PAYLOAD_SIZE;
	if(kind == APBRIDGE_PACKET_WRITE_TX && request->error == 0)
	{
		if(size != (u32)(APBRIDGE_TX_REQUEST_SIZE + request->length))
			request->error = APBRIDGE_WRITE_BAD_PAYLOAD_SIZE;
		else if((request->txHigh | request->txLow) == 0)
			request->error = APBRIDGE_TX_BAD_ID;
	}
	return kind;
}

static void APBridge_EncodeMemoryReply(u8 *reply,
	const struct APBridgeMemoryRequest *request, enum APBridgePacketKind kind)
{
	const char *magic = request->error ? "ERR!"
		: (kind == APBRIDGE_PACKET_READ ? "DATA" : "WROK");
	reply[0] = magic[0];
	reply[1] = magic[1];
	reply[2] = magic[2];
	reply[3] = magic[3];
	APBridge_WriteBE32(reply + 4, request->address);
	APBridge_WriteBE16(reply + 8, request->length);
	reply[10] = request->error;
	reply[11] = 0;
	if(kind == APBRIDGE_PACKET_WRITE_TX)
	{
		APBridge_WriteBE32(reply + 12, request->txHigh);
		APBridge_WriteBE32(reply + 16, request->txLow);
	}
}

static enum APBridgeTxLookupResult APBridge_TxCacheLookup(
	const struct APBridgeTxCache *cache,
	const struct APBridgeMemoryRequest *request, const u8 *payload)
{
	/* No peer key: a retransmission may arrive from a new UDP source port. */
	u32 i;
	const struct APBridgeTxEntry *entry;

	for(i = 0; i < APBRIDGE_TX_CACHE_ENTRIES; i++)
	{
		entry = &cache->entries[i];
		if(entry->txHigh != request->txHigh || entry->txLow != request->txLow)
			continue;
		if(entry->address == request->address && entry->length == request->length
			&& memcmp(entry->payload, payload, request->length) == 0)
			return APBRIDGE_TX_DUPLICATE;
		return APBRIDGE_TX_ID_CONFLICT;
	}
	return APBRIDGE_TX_NEW;
}

static void APBridge_TxCacheRecord(struct APBridgeTxCache *cache,
	const struct APBridgeMemoryRequest *request, const u8 *payload)
{
	struct APBridgeTxEntry *entry;

	if(request->error != 0 || (request->txHigh | request->txLow) == 0
		|| request->length == 0 || request->length > APBRIDGE_WRITE_MAX_LENGTH)
		return;
	entry = &cache->entries[cache->next];
	entry->txHigh = 0;
	entry->txLow = 0;
	entry->address = request->address;
	entry->length = request->length;
	memcpy(entry->payload, payload, request->length);
	entry->txHigh = request->txHigh;
	entry->txLow = request->txLow;
	cache->next = (cache->next + 1) % APBRIDGE_TX_CACHE_ENTRIES;
}

#endif
