#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#include "../kernel/APBridgeProtocol.h"

static void make_read(u8 *packet, u32 address, u16 length, u16 reserved)
{
	memcpy(packet, "READ", 4);
	packet[4] = (u8)(address >> 24);
	packet[5] = (u8)(address >> 16);
	packet[6] = (u8)(address >> 8);
	packet[7] = (u8)address;
	packet[8] = (u8)(length >> 8);
	packet[9] = (u8)length;
	packet[10] = (u8)(reserved >> 8);
	packet[11] = (u8)reserved;
}

static void check_read(u32 address, u16 length, u16 reserved, u8 error)
{
	u8 packet[APBRIDGE_READ_REQUEST_SIZE];
	struct APBridgeMemoryRequest request;

	make_read(packet, address, length, reserved);
	assert(APBridge_DecodePacket(packet, sizeof(packet), &request)
		== APBRIDGE_PACKET_READ);
	assert(request.address == address);
	assert(request.length == length);
	assert(request.error == error);
}

static void check_write(u32 address, u16 length, u16 reserved,
	u32 payload_size, u8 error)
{
	u8 packet[APBRIDGE_PACKET_BUFFER_SIZE];
	struct APBridgeMemoryRequest request;

	assert(APBRIDGE_WRITE_REQUEST_SIZE + payload_size <= sizeof(packet));
	make_read(packet, address, length, reserved);
	memcpy(packet, "WRIT", 4);
	memset(packet + APBRIDGE_WRITE_REQUEST_SIZE, 0x5A, payload_size);
	assert(APBridge_DecodePacket(packet,
		APBRIDGE_WRITE_REQUEST_SIZE + payload_size, &request)
		== APBRIDGE_PACKET_WRITE);
	assert(request.address == address);
	assert(request.length == length);
	assert(request.error == error);
}

static void make_tx(u8 *packet, u32 address, u16 length, u16 reserved,
	u32 txHigh, u32 txLow, u32 payload_size)
{
	make_read(packet, address, length, reserved);
	memcpy(packet, "WRTX", 4);
	packet[12] = (u8)(txHigh >> 24);
	packet[13] = (u8)(txHigh >> 16);
	packet[14] = (u8)(txHigh >> 8);
	packet[15] = (u8)txHigh;
	packet[16] = (u8)(txLow >> 24);
	packet[17] = (u8)(txLow >> 16);
	packet[18] = (u8)(txLow >> 8);
	packet[19] = (u8)txLow;
	memset(packet + APBRIDGE_TX_REQUEST_SIZE, 0x5A, payload_size);
}

static void check_tx(u32 address, u16 length, u16 reserved,
	u32 txHigh, u32 txLow, u32 payload_size, u8 error)
{
	u8 packet[APBRIDGE_PACKET_BUFFER_SIZE];
	struct APBridgeMemoryRequest request;

	assert(APBRIDGE_TX_REQUEST_SIZE + payload_size <= sizeof(packet));
	make_tx(packet, address, length, reserved, txHigh, txLow, payload_size);
	assert(APBridge_DecodePacket(packet,
		APBRIDGE_TX_REQUEST_SIZE + payload_size, &request)
		== APBRIDGE_PACKET_WRITE_TX);
	assert(request.address == address);
	assert(request.length == length);
	assert(request.txHigh == txHigh && request.txLow == txLow);
	assert(request.error == error);
}

static void test_tx_cache(void)
{
	struct APBridgeTxCache cache;
	struct APBridgeMemoryRequest request;
	u8 payload[APBRIDGE_WRITE_MAX_LENGTH];
	u32 writes, i;
	u16 sourcePort;

	assert(sizeof(struct APBridgeTxEntry) == 272);
	assert(sizeof(cache) == 4356);
	memset(&cache, 0, sizeof(cache));
	memset(&request, 0, sizeof(request));
	memset(payload, 0, sizeof(payload));
	request.address = 0x80000100U;
	request.length = 1;
	request.txHigh = 0x01234567U;
	request.txLow = 0x89ABCDEFU;
	payload[0] = 0xA5;
	writes = 0;
	sourcePort = 41000;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_NEW);
	if(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_NEW)
	{
		writes++;
		APBridge_TxCacheRecord(&cache, &request, payload);
	}
	/* A retry from another UDP port uses this same peer-independent lookup. */
	sourcePort = 42000;
	assert(sourcePort != 41000);
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_DUPLICATE);
	assert(writes == 1);
	request.address++;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_ID_CONFLICT);
	request.address--;
	request.length = 2;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_ID_CONFLICT);
	request.length = 1;
	payload[0] = 0xA6;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_ID_CONFLICT);
	payload[0] = 0xA5;

	request.error = APBRIDGE_TX_BAD_ID;
	request.txLow++;
	APBridge_TxCacheRecord(&cache, &request, payload);
	assert(cache.next == 1);
	request.error = 0;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_NEW);
	request.txHigh = 0;
	request.txLow = 0;
	APBridge_TxCacheRecord(&cache, &request, payload);
	assert(cache.next == 1);

	memset(&cache, 0, sizeof(cache));
	request.txHigh = 0;
	for(i = 1; i <= APBRIDGE_TX_CACHE_ENTRIES; i++)
	{
		request.txLow = i;
		APBridge_TxCacheRecord(&cache, &request, payload);
	}
	assert(cache.next == 0);
	request.txLow = 1;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_DUPLICATE);
	request.txLow = APBRIDGE_TX_CACHE_ENTRIES + 1;
	APBridge_TxCacheRecord(&cache, &request, payload);
	assert(cache.next == 1);
	request.txLow = 1;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_NEW);
	request.txLow = 2;
	assert(APBridge_TxCacheLookup(&cache, &request, payload) == APBRIDGE_TX_DUPLICATE);
}

int main(void)
{
	u8 packet[APBRIDGE_READ_REQUEST_SIZE];
	u8 write_packet[APBRIDGE_WRITE_REQUEST_SIZE + 2];
	u8 tx_packet[APBRIDGE_TX_REQUEST_SIZE + 2];
	u8 reply[APBRIDGE_READ_REPLY_SIZE];
	struct APBridgeMemoryRequest request;
	static const u8 expected_data_header[12] = {
		'D', 'A', 'T', 'A', 0x80, 0, 0, 0, 0, 6, 0, 0
	};
	static const u8 expected_error_header[12] = {
		'E', 'R', 'R', '!', 0x80, 0, 0, 0, 0, 0, 1, 0
	};
	static const u8 expected_write_header[12] = {
		'W', 'R', 'O', 'K', 0x81, 0x23, 0x45, 0x67, 0, 2, 0, 0
	};
	static const u8 expected_tx_header[20] = {
		'W', 'R', 'O', 'K', 0x81, 0x23, 0x45, 0x67, 0, 2, 0, 0,
		0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
	};
	static const u8 expected_tx_error[20] = {
		'E', 'R', 'R', '!', 0x81, 0x23, 0x45, 0x67, 0, 2, 6, 0,
		0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
	};

	/* Real request bytes for 0x80000000, length 6: decode and echo. */
	memcpy(packet, "READ\x80\0\0\0\0\x06\0\0", sizeof(packet));
	assert(APBridge_DecodePacket(packet, sizeof(packet), &request)
		== APBRIDGE_PACKET_READ);
	assert(request.address == 0x80000000U && request.length == 6);
	assert(request.error == 0);
	memset(reply, 0xAA, sizeof(reply));
	APBridge_EncodeMemoryReply(reply, &request, APBRIDGE_PACKET_READ);
	assert(memcmp(reply, expected_data_header, sizeof(expected_data_header)) == 0);
	assert(reply[12] == 0xAA); /* Payload starts after the header. */

	check_read(0x80000000U, 0, 0, APBRIDGE_READ_BAD_LENGTH);
	check_read(0x80000000U, 257, 0, APBRIDGE_READ_BAD_LENGTH);
	check_read(0x7FFFFFFFU, 1, 0, APBRIDGE_READ_BAD_RANGE);
	check_read(0x817FFFFFU, 1, 0, 0);
	check_read(0x817FFFFFU, 2, 0, APBRIDGE_READ_BAD_RANGE);
	check_read(0x817FFF00U, 256, 0, 0);
	check_read(0x81800000U, 1, 0, APBRIDGE_READ_BAD_RANGE);
	check_read(0xFFFFFFFFU, 2, 0, APBRIDGE_READ_BAD_RANGE);
	check_read(0x80000000U, 6, 1, APBRIDGE_READ_BAD_RESERVED);

	make_read(packet, 0x80000000U, 0, 0);
	assert(APBridge_DecodePacket(packet, sizeof(packet), &request)
		== APBRIDGE_PACKET_READ);
	APBridge_EncodeMemoryReply(reply, &request, APBRIDGE_PACKET_READ);
	assert(memcmp(reply, expected_error_header, sizeof(expected_error_header)) == 0);
	APBridge_EncodeMemoryReply(reply, &request, APBRIDGE_PACKET_WRITE);
	assert(memcmp(reply, expected_error_header, sizeof(expected_error_header)) == 0);

	check_write(0x80000000U, 1, 0, 1, 0);
	check_write(0x80000000U, 256, 0, 256, 0);
	check_write(0x80000000U, 0, 0, 0, APBRIDGE_READ_BAD_LENGTH);
	check_write(0x80000000U, 257, 0, 0, APBRIDGE_READ_BAD_LENGTH);
	check_write(0x80000000U, 257, 0, 257, APBRIDGE_READ_BAD_LENGTH);
	check_write(0x7FFFFFFFU, 1, 0, 1, APBRIDGE_READ_BAD_RANGE);
	check_write(0x81800000U, 1, 0, 1, APBRIDGE_READ_BAD_RANGE);
	check_write(0x817FFFFFU, 1, 0, 1, 0);
	check_write(0x817FFFFFU, 2, 0, 2, APBRIDGE_READ_BAD_RANGE);
	check_write(0x817FFF00U, 256, 0, 256, 0);
	check_write(0xFFFFFFFFU, 1, 0, 1, APBRIDGE_READ_BAD_RANGE);
	check_write(0x80000000U, 1, 1, 1, APBRIDGE_READ_BAD_RESERVED);
	check_write(0x80000000U, 1, 0, 2, APBRIDGE_WRITE_BAD_PAYLOAD_SIZE);
	check_write(0x80000000U, 2, 0, 1, APBRIDGE_WRITE_BAD_PAYLOAD_SIZE);
	check_write(0x80000000U, 256, 0, 257, APBRIDGE_WRITE_BAD_PAYLOAD_SIZE);

	make_read(write_packet, 0x81234567U, 2, 0);
	memcpy(write_packet, "WRIT", 4);
	write_packet[12] = 0x12;
	write_packet[13] = 0x34;
	assert(APBridge_DecodePacket(write_packet, sizeof(write_packet),
		&request) == APBRIDGE_PACKET_WRITE);
	assert(request.error == 0);
	APBridge_EncodeMemoryReply(reply, &request, APBRIDGE_PACKET_WRITE);
	assert(memcmp(reply, expected_write_header, sizeof(expected_write_header)) == 0);
	assert(reply[12] == 0xAA); /* Legacy responses remain 12 bytes. */

	check_tx(0x80000000U, 1, 0, 1, 2, 1, 0);
	check_tx(0x80000000U, 256, 0, 1, 2, 256, 0);
	check_tx(0x817FFFFFU, 1, 0, 1, 2, 1, 0);
	check_tx(0x817FFF00U, 256, 0, 1, 2, 256, 0);
	check_tx(0x80000000U, 0, 0, 1, 2, 0, APBRIDGE_READ_BAD_LENGTH);
	check_tx(0x80000000U, 257, 0, 1, 2, 0, APBRIDGE_READ_BAD_LENGTH);
	check_tx(0x7FFFFFFFU, 1, 0, 1, 2, 1, APBRIDGE_READ_BAD_RANGE);
	check_tx(0x81800000U, 1, 0, 1, 2, 1, APBRIDGE_READ_BAD_RANGE);
	check_tx(0xFFFFFFFFU, 1, 0, 1, 2, 1, APBRIDGE_READ_BAD_RANGE);
	check_tx(0x817FFFFFU, 2, 0, 1, 2, 2, APBRIDGE_READ_BAD_RANGE);
	check_tx(0x80000000U, 1, 1, 1, 2, 1, APBRIDGE_READ_BAD_RESERVED);
	check_tx(0x80000000U, 1, 0, 0, 0, 1, APBRIDGE_TX_BAD_ID);
	check_tx(0x80000000U, 2, 0, 1, 2, 1, APBRIDGE_WRITE_BAD_PAYLOAD_SIZE);
	check_tx(0x80000000U, 1, 0, 1, 2, 2, APBRIDGE_WRITE_BAD_PAYLOAD_SIZE);
	check_tx(0x80000000U, 256, 0, 1, 2, 257, APBRIDGE_WRITE_BAD_PAYLOAD_SIZE);
	make_tx(tx_packet, 0x81234567U, 2, 0, 0x01234567U, 0x89ABCDEFU, 2);
	assert(APBridge_DecodePacket(tx_packet, sizeof(tx_packet), &request)
		== APBRIDGE_PACKET_WRITE_TX && request.error == 0);
	APBridge_EncodeMemoryReply(reply, &request, APBRIDGE_PACKET_WRITE_TX);
	assert(memcmp(reply, expected_tx_header, sizeof(expected_tx_header)) == 0);
	request.error = APBRIDGE_TX_CONFLICT;
	APBridge_EncodeMemoryReply(reply, &request, APBRIDGE_PACKET_WRITE_TX);
	assert(memcmp(reply, expected_tx_error, sizeof(expected_tx_error)) == 0);
	test_tx_cache();

	assert(APBridge_DecodePacket((const u8*)"HELLO", 5, &request)
		== APBRIDGE_PACKET_HELLO);
	assert(APBridge_DecodePacket((const u8*)"HELLO!", 6, &request)
		== APBRIDGE_PACKET_IGNORE);
	assert(APBridge_DecodePacket((const u8*)"WHAT", 4, &request)
		== APBRIDGE_PACKET_IGNORE);
	assert(APBridge_DecodePacket((const u8*)"READ", 4, &request)
		== APBRIDGE_PACKET_IGNORE);
	assert(APBridge_DecodePacket((const u8*)"READxxxxxxxxx", 13, &request)
		== APBRIDGE_PACKET_IGNORE);
	assert(APBridge_DecodePacket((const u8*)"WRIT", 4, &request)
		== APBRIDGE_PACKET_IGNORE);
	assert(APBridge_DecodePacket((const u8*)"WRIXxxxxxxxx", 12, &request)
		== APBRIDGE_PACKET_IGNORE);
	assert(APBridge_DecodePacket((const u8*)"WRTXxxxxxxxxxxxxxxx", 19, &request)
		== APBRIDGE_PACKET_IGNORE);

	puts("AP Bridge protocol tests passed");
	return 0;
}
