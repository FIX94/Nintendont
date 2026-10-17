#include <assert.h>
#include <string.h>

#include "SwitchProIncoming.h"
#include "BTChannelEligibility.h"
#include "SwitchProPairing.h"

static void make_pairing(SwitchProPairing *pairing)
{
	u32 i, j;
	memset(pairing, 0, sizeof(*pairing));
	pairing->magic = SWITCH_PRO_PAIRING_MAGIC;
	pairing->version = SWITCH_PRO_PAIRING_VERSION;
	pairing->size = sizeof(*pairing);
	pairing->count = 2;
	for(i = 0; i < 6; i++)
	{
		pairing->console_bda[i] = i + 11;
		pairing->controllers[0].controller_bda[i] = i + 1;
		pairing->controllers[1].controller_bda[i] = i + 21;
	}
	for(i = 0; i < pairing->count; i++)
	{
		pairing->controllers[i].controller_type = SWITCH_PRO_PAIRING_TYPE;
		pairing->controllers[i].vendor_id = 0x057e;
		pairing->controllers[i].product_id = 0x2009;
		pairing->controllers[i].key_type = SWITCH_PRO_PAIRING_KEY_TYPE_UNKNOWN;
		for(j = 0; j < 16; j++)
			pairing->controllers[i].hci_link_key[j] = i + j + 31;
	}
	pairing->checksum = SwitchProPairingChecksum(pairing);
}

static void test_pairing_record(void)
{
	SwitchProPairing pairing;
	SwitchProPairing mixed;
	SwitchProPairing filtered;
	SwitchProPairing upgraded;
	SwitchProPairingLegacyV2 legacy;
	u8 lwbt[6];
	u8 hci_key[16];
	const u8 expected[6] = {6, 5, 4, 3, 2, 1};
	make_pairing(&pairing);
	assert(sizeof(pairing) == 140);
	assert(SWITCH_PRO_CANONICAL_CONTROLLER == 1);
	assert((SWITCH_PRO_PAIRING_PPC_ADDR & 0x1fffffffu) ==
		SWITCH_PRO_PAIRING_ARM_ADDR);
	assert(SwitchProPairingIsValid(&pairing));
	SwitchProPairingAddressToLwbt(pairing.controllers[0].controller_bda, lwbt);
	assert(memcmp(lwbt, expected, sizeof(lwbt)) == 0);
	SwitchProPairingCopyHciLinkKey(&pairing.controllers[0], hci_key);
	assert(memcmp(hci_key, pairing.controllers[0].hci_link_key, sizeof(hci_key)) == 0);
	pairing.controllers[0].hci_link_key[3] ^= 0x80;
	assert(!SwitchProPairingIsValid(&pairing));
	pairing.controllers[0].hci_link_key[3] ^= 0x80;
	pairing.checksum = SwitchProPairingChecksum(&pairing);
	pairing.version = 1;
	assert(!SwitchProPairingIsValid(&pairing));
	pairing.version = SWITCH_PRO_PAIRING_VERSION;
	pairing.checksum = SwitchProPairingChecksum(&pairing);
	pairing.controllers[0].product_id = 0x2008;
	assert(!SwitchProPairingIsValid(&pairing));

	make_pairing(&mixed);
	mixed.count = 3;
	mixed.controllers[2] = mixed.controllers[0];
	mixed.controllers[2].controller_bda[0] = 0x55;
	mixed.controllers[2].vendor_id = 0;
	mixed.controllers[2].product_id = 0;
	mixed.checksum = SwitchProPairingChecksum(&mixed);
	assert(!SwitchProPairingIsValid(&mixed));
	assert(SwitchProPairingFilterSupported(&mixed, &filtered));
	assert(filtered.count == 2);
	assert(SwitchProPairingIsValid(&filtered));

	/* Unsupported entry order must not affect the supported result. */
	mixed.controllers[2] = mixed.controllers[1];
	mixed.controllers[1] = mixed.controllers[0];
	mixed.controllers[0].controller_bda[0] = 0x55;
	mixed.controllers[0].vendor_id = 0;
	mixed.controllers[0].product_id = 0;
	mixed.checksum = SwitchProPairingChecksum(&mixed);
	assert(SwitchProPairingFilterSupported(&mixed, &filtered));
	assert(filtered.count == 2);
	assert(SwitchProPairingIsValid(&filtered));

	/* Envelope corruption remains fatal even when supported entries exist. */
	mixed.checksum ^= 1;
	assert(!SwitchProPairingFilterSupported(&mixed, &filtered));
	mixed.checksum ^= 1;
	mixed.controllers[1].hci_link_key[0] = 0;
	memset(mixed.controllers[1].hci_link_key, 0,
		sizeof(mixed.controllers[1].hci_link_key));
	mixed.checksum = SwitchProPairingChecksum(&mixed);
	assert(!SwitchProPairingFilterSupported(&mixed, &filtered));

	memset(&legacy, 0, sizeof(legacy));
	legacy.magic = SWITCH_PRO_PAIRING_MAGIC;
	legacy.version = 2;
	legacy.size = sizeof(legacy);
	legacy.controller_bda[0] = 1;
	legacy.console_bda[0] = 2;
	legacy.controller_type = SWITCH_PRO_PAIRING_TYPE;
	legacy.vendor_id = 0x057e;
	legacy.product_id = 0x2009;
	legacy.key_type = SWITCH_PRO_PAIRING_KEY_TYPE_UNKNOWN;
	memset(legacy.hci_link_key, 0x5a, sizeof(legacy.hci_link_key));
	legacy.checksum = SwitchProPairingLegacyV2Checksum(&legacy);
	assert(SwitchProPairingUpgradeLegacyV2(&legacy, &upgraded));
	assert(upgraded.count == 1);
	assert(SwitchProPairingIsValid(&upgraded));
	assert(memcmp(upgraded.controllers[0].controller_bda,
		legacy.controller_bda, 6) == 0);
	legacy.checksum ^= 1;
	assert(!SwitchProPairingUpgradeLegacyV2(&legacy, &upgraded));
}

static void test_registration_is_independent_and_idempotent(void)
{
	struct SwitchProIncomingState state;
	SwitchProIncomingReset(&state);
	assert(!SwitchProIncomingReady(&state));
	assert(!SwitchProIncomingNeedsListener(&state));
	SwitchProIncomingImported(&state);
	assert(SwitchProIncomingNeedsListener(&state));
	SwitchProIncomingListener(&state, -1);
	assert(!state.listener_registered);
	assert(SwitchProIncomingNeedsListener(&state));
	SwitchProIncomingListener(&state, 0);
	SwitchProIncomingListener(&state, 0);
	assert(state.listener_registered == 1);
	assert(!SwitchProIncomingNeedsListener(&state));
}

static void test_transport_both_channel_orders(void)
{
	struct SwitchProIncomingState state;
	u32 reverse;
	for(reverse = 0; reverse < 2; reverse++)
	{
		SwitchProIncomingReset(&state);
		SwitchProIncomingImported(&state);
		SwitchProIncomingListener(&state, 0);
		SwitchProIncomingACL(&state, 0);
		SwitchProIncomingEncryption(&state, 0, 1);
		assert(state.authenticated);
		if(reverse)
		{
			SwitchProIncomingChannels(&state, 0, 1);
			assert(!SwitchProIncomingReady(&state));
			SwitchProIncomingChannels(&state, 1, 1);
		}
		else
		{
			SwitchProIncomingChannels(&state, 1, 0);
			assert(!SwitchProIncomingReady(&state));
			SwitchProIncomingChannels(&state, 1, 1);
		}
		assert(SwitchProIncomingReady(&state));
		/* Channel callbacks alone are not proof that BTE/L2CAP is ready
		 * to transmit.  Finalization must wait for the later pump. */
		assert(!SwitchProIncomingNeedsFinalize(&state));
		SwitchProIncomingTransport(&state,0);
		assert(!SwitchProIncomingNeedsFinalize(&state));
		SwitchProIncomingTransport(&state,1);
		assert(SwitchProIncomingNeedsFinalize(&state));
		SwitchProIncomingFinalized(&state);
		SwitchProIncomingFinalized(&state);
		assert(state.finalized == 1);
		assert(!SwitchProIncomingNeedsFinalize(&state));
	}
}

static void test_generic_channel_requires_valid_input(void)
{
	u8 valid[11] = { 0x3d };
	u8 wrong_report[11] = { 0x30 };

	assert(!BTGenericReportActivatesChannel(0x3d, valid, 10));
	assert(!BTGenericReportActivatesChannel(0x34, valid, sizeof(valid)));
	assert(!BTGenericReportActivatesChannel(0x3d, wrong_report,
		sizeof(wrong_report)));
	assert(BTGenericReportActivatesChannel(0x3d, valid, sizeof(valid)));
}

static void test_encrypted_reconnect_survives_redundant_auth_failure(void)
{
	struct SwitchProIncomingState state;
	SwitchProIncomingReset(&state);
	SwitchProIncomingImported(&state);
	SwitchProIncomingListener(&state, 0);
	SwitchProIncomingACL(&state, 0);
	SwitchProIncomingEncryption(&state, 0, 1);
	SwitchProIncomingAuthentication(&state, 5);
	SwitchProIncomingChannels(&state, 1, 1);
	assert(state.authenticated);
	assert(SwitchProIncomingReady(&state));
}

static void test_security_after_channels_also_finalizes(void)
{
	struct SwitchProIncomingState state;
	SwitchProIncomingReset(&state);
	SwitchProIncomingImported(&state);
	SwitchProIncomingListener(&state, 0);
	SwitchProIncomingACL(&state, 0);
	SwitchProIncomingChannels(&state, 1, 1);
	assert(!SwitchProIncomingReady(&state));
	SwitchProIncomingEncryption(&state, 0, 1);
	assert(SwitchProIncomingReady(&state));
	assert(!SwitchProIncomingNeedsFinalize(&state));
	SwitchProIncomingTransport(&state,1);
	assert(SwitchProIncomingNeedsFinalize(&state));
}

static void test_encryption_failure_blocks_hid(void)
{
	struct SwitchProIncomingState state;
	SwitchProIncomingReset(&state);
	SwitchProIncomingImported(&state);
	SwitchProIncomingListener(&state, 0);
	SwitchProIncomingACL(&state, 0);
	SwitchProIncomingAuthentication(&state, 5);
	SwitchProIncomingEncryption(&state, 5, 0);
	SwitchProIncomingChannels(&state, 1, 1);
	assert(!state.authenticated);
	assert(!SwitchProIncomingReady(&state));
}

static void test_full_left_y_gamecube_direction(void)
{
	struct SwitchProIncomingState state;
	u8 report[49];

	memset(report, 0, sizeof(report));
	report[0] = SWITCH_PRO_REPORT_FULL;
	report[7] = 0xf8;
	report[8] = 0xff;
	SwitchProIncomingReset(&state);
	state.identity_confirmed = 1;
	assert(SwitchProIncomingHandleReport(&state, report, sizeof(report)) ==
		SWITCH_PRO_EVENT_INPUT);
	assert(state.input.left_y == 127);

	report[7] = 0x08;
	report[8] = 0x00;
	assert(SwitchProIncomingHandleReport(&state, report, sizeof(report)) ==
		SWITCH_PRO_EVENT_INPUT);
	assert(state.input.left_y == -128);
}

static void make_ready(struct SwitchProIncomingState *state)
{
	SwitchProIncomingReset(state);
	SwitchProIncomingImported(state);
	SwitchProIncomingListener(state, 0);
	SwitchProIncomingACL(state, 0);
	SwitchProIncomingEncryption(state, 0, 1);
	SwitchProIncomingChannels(state, 1, 1);
	SwitchProIncomingTransport(state, 1);
	SwitchProIncomingFinalized(state);
}

static void test_exact_linux_init_sequence(void)
{
	static const u8 commands[SWITCH_PRO_INIT_COMMAND_COUNT] = {
		0x02, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x40, 0x03, 0x48, 0x30, 0x38
	};
	static const u8 lengths[SWITCH_PRO_INIT_COMMAND_COUNT] = {
		0, 5, 5, 5, 5, 5, 5, 1, 1, 1, 1, 5
	};
	static const u8 data[SWITCH_PRO_INIT_COMMAND_COUNT][5] = {
		{0, 0, 0, 0, 0},
		{0x10, 0x80, 0x00, 0x00, 0x02},
		{0x1b, 0x80, 0x00, 0x00, 0x02},
		{0x12, 0x80, 0x00, 0x00, 0x09},
		{0x1d, 0x80, 0x00, 0x00, 0x09},
		{0x26, 0x80, 0x00, 0x00, 0x02},
		{0x28, 0x80, 0x00, 0x00, 0x18},
		{0x01, 0, 0, 0, 0},
		{0x30, 0, 0, 0, 0},
		{0x01, 0, 0, 0, 0},
		{0x01, 0, 0, 0, 0},
		{0x01, 0x00, 0x00, 0x11, 0x11}
	};
	struct SwitchProIncomingState state;
	u8 report[64];
	u8 reply[50];
	u32 i;
	u16 len;

	make_ready(&state);
	SwitchProIncomingSetChannel(&state, 0);
	SwitchProIncomingStartInit(&state);
	assert(state.init_started);
	for(i = 0; i < SWITCH_PRO_INIT_COMMAND_COUNT; i++)
	{
		memset(report, 0xa5, sizeof(report));
		len = SwitchProIncomingBuildInit(&state, report,
			sizeof(report), 0);
		assert(len == (u16)(11 + lengths[i]));
		assert(report[0] == 0x01);
		assert(report[1] == i);
		assert(memcmp(&report[2], "\0\0\0\0\0\0\0\0", 8) == 0);
		assert(report[10] == commands[i]);
		assert(memcmp(&report[11], data[i], lengths[i]) == 0);
		assert(!SwitchProIncomingBuildInit(&state, report,
			sizeof(report), 0));

		memset(reply, 0, sizeof(reply));
		reply[0] = SWITCH_PRO_REPORT_COMMAND;
		reply[13] = i == 0 ? 0x82 : 0x80;
		reply[14] = commands[i];
		if(commands[i] == 0x10)
			memcpy(&reply[15], data[i], 5);
		assert(SwitchProIncomingHandleReport(&state, reply,
			sizeof(reply)) == SWITCH_PRO_EVENT_ACK);
	}
	assert(state.identity_confirmed);
	assert(state.init_complete);
	assert(!SwitchProIncomingBuildInit(&state, report, sizeof(report), 0));
}

static void test_player_led_channel_mapping_and_updates(void)
{
	struct SwitchProIncomingState state;
	u8 report[16];
	u8 reply[15];
	u32 i;

	assert(SwitchProIncomingPlayerLedMask(0) == 0x01);
	assert(SwitchProIncomingPlayerLedMask(1) == 0x03);
	assert(SwitchProIncomingPlayerLedMask(2) == 0x07);
	assert(SwitchProIncomingPlayerLedMask(3) == 0x0f);
	assert(SwitchProIncomingPlayerLedMask(4) == 0x00);

	make_ready(&state);
	SwitchProIncomingSetChannel(&state, 1);
	SwitchProIncomingStartInit(&state);
	for(i = 0; i < SWITCH_PRO_INIT_COMMAND_COUNT; i++)
	{
		u16 len = SwitchProIncomingBuildInit(&state, report,
			sizeof(report), 0);
		assert(len != 0);
		if(report[10] == 0x30)
			assert(report[11] == 0x03);
		memset(reply, 0, sizeof(reply));
		reply[0] = SWITCH_PRO_REPORT_COMMAND;
		reply[13] = 0x80;
		reply[14] = report[10];
		if(report[10] == 0x10)
		{
			u8 spi_reply[20] = {0};
			memcpy(spi_reply, reply, sizeof(reply));
			memcpy(&spi_reply[15], &report[11], 5);
			assert(SwitchProIncomingHandleReport(&state, spi_reply,
				sizeof(spi_reply)) == SWITCH_PRO_EVENT_ACK);
		}
		else
			assert(SwitchProIncomingHandleReport(&state, reply,
				sizeof(reply)) == SWITCH_PRO_EVENT_ACK);
	}
	assert(state.applied_led_mask == 0x03);
	assert(!SwitchProIncomingNeedsLedUpdate(&state));

	SwitchProIncomingSetChannel(&state, 3);
	assert(SwitchProIncomingNeedsLedUpdate(&state));
	assert(SwitchProIncomingBuildLedUpdate(&state, report,
		sizeof(report), 0) == 12);
	assert(report[10] == 0x30 && report[11] == 0x0f);
	assert(!SwitchProIncomingNeedsLedUpdate(&state));
	memset(reply, 0, sizeof(reply));
	reply[0] = SWITCH_PRO_REPORT_COMMAND;
	reply[13] = 0x80;
	reply[14] = 0x30;
	assert(SwitchProIncomingHandleReport(&state, reply, sizeof(reply)) ==
		SWITCH_PRO_EVENT_ACK);
	assert(state.applied_led_mask == 0x0f);
	assert(!SwitchProIncomingNeedsLedUpdate(&state));
	assert(!SwitchProIncomingBuildLedUpdate(&state, report,
		sizeof(report), 0));
}

static void test_init_led_waits_for_channel_then_resumes(void)
{
	struct SwitchProIncomingState state;
	u8 report[64];
	u8 reply[64];
	u32 i;

	make_ready(&state);
	SwitchProIncomingStartInit(&state);
	for(i = 0; i < 10; i++)
	{
		u16 len = SwitchProIncomingBuildInit(&state, report,
			sizeof(report), 0);
		assert(len != 0);
		memset(reply, 0, sizeof(reply));
		reply[0] = SWITCH_PRO_REPORT_COMMAND;
		reply[13] = 0x80;
		reply[14] = report[10];
		if(report[10] == 0x10)
			memcpy(&reply[15], &report[11], 5);
		assert(SwitchProIncomingHandleReport(&state, reply,
			sizeof(reply)) == SWITCH_PRO_EVENT_ACK);
	}
	assert(state.init_index == 10);
	assert(!state.awaiting_ack);
	assert(!SwitchProIncomingBuildInit(&state, report,
		sizeof(report), 0));
	assert(!state.init_failed);
	SwitchProIncomingSetChannel(&state, 0);
	assert(SwitchProIncomingBuildInit(&state, report,
		sizeof(report), 0) == 12);
	assert(report[10] == 0x30);
	assert(report[11] == 0x01);
}

static void test_player_led_retry_is_bounded_and_reconnect_resets(void)
{
	struct SwitchProIncomingState state;
	u8 report[16];
	u32 i;

	memset(&state, 0, sizeof(state));
	state.init_complete = 1;
	SwitchProIncomingSetChannel(&state, 2);
	assert(SwitchProIncomingBuildLedUpdate(&state, report,
		sizeof(report), 0) == 12);
	assert(report[11] == 0x07);
	for(i = 0; i < SWITCH_PRO_INIT_RETRY_MAX; i++)
	{
		assert(SwitchProIncomingBuildLedUpdate(&state, report,
			sizeof(report), 1) == 12);
		assert(report[11] == 0x07);
	}
	assert(!SwitchProIncomingBuildLedUpdate(&state, report,
		sizeof(report), 1));
	assert(state.led_failed);
	assert(!state.led_awaiting_ack);
	assert(!SwitchProIncomingNeedsLedUpdate(&state));

	SwitchProIncomingReset(&state);
	assert(state.desired_led_mask == 0);
	assert(state.applied_led_mask == 0);
	SwitchProIncomingSetChannel(&state, 0);
	assert(state.desired_led_mask == 0x01);
	assert(!state.led_failed);
}

static void test_init_retries_and_ack_validation(void)
{
	struct SwitchProIncomingState state;
	u8 report[16];
	u8 reply[15];
	u32 i;

	make_ready(&state);
	SwitchProIncomingStartInit(&state);
	assert(SwitchProIncomingBuildInit(&state, report, sizeof(report), 0) == 11);
	memset(reply, 0, sizeof(reply));
	reply[0] = SWITCH_PRO_REPORT_COMMAND;
	reply[13] = 0x00;
	reply[14] = 0x02;
	assert(SwitchProIncomingHandleReport(&state, reply, sizeof(reply)) ==
		SWITCH_PRO_EVENT_NEGATIVE_ACK);
	assert(state.init_index == 0 && state.awaiting_ack);
	for(i = 0; i < SWITCH_PRO_INIT_RETRY_MAX; i++)
		assert(SwitchProIncomingBuildInit(&state, report,
			sizeof(report), 1) == 11);
	assert(!SwitchProIncomingBuildInit(&state, report, sizeof(report), 1));
	assert(state.init_failed);
}

static void test_delayed_spi_ack_does_not_advance_next_read(void)
{
	struct SwitchProIncomingState state;
	u8 report[16];
	u8 reply[50];
	static const u8 first_spi[5] = {0x10, 0x80, 0x00, 0x00, 0x02};
	static const u8 second_spi[5] = {0x1b, 0x80, 0x00, 0x00, 0x02};

	make_ready(&state);
	SwitchProIncomingStartInit(&state);
	assert(SwitchProIncomingBuildInit(&state, report, sizeof(report), 0) == 11);
	memset(reply, 0, sizeof(reply));
	reply[0] = SWITCH_PRO_REPORT_COMMAND;
	reply[13] = 0x82;
	reply[14] = 0x02;
	assert(SwitchProIncomingHandleReport(&state, reply, sizeof(reply)) ==
		SWITCH_PRO_EVENT_ACK);
	assert(SwitchProIncomingBuildInit(&state, report, sizeof(report), 0) == 16);

	memset(reply, 0, sizeof(reply));
	reply[0] = SWITCH_PRO_REPORT_COMMAND;
	reply[13] = 0x90;
	reply[14] = 0x10;
	memcpy(&reply[15], second_spi, sizeof(second_spi));
	assert(SwitchProIncomingHandleReport(&state, reply, sizeof(reply)) ==
		SWITCH_PRO_EVENT_NONE);
	assert(state.init_index == 1 && state.awaiting_ack);
	memcpy(&reply[15], first_spi, sizeof(first_spi));
	assert(SwitchProIncomingHandleReport(&state, reply, sizeof(reply)) ==
		SWITCH_PRO_EVENT_ACK);
	assert(state.init_index == 2 && !state.awaiting_ack);
}

static void test_full_report_end_to_end(void)
{
	struct SwitchProIncomingState state;
	u8 command[16];
	u8 reply[50];
	u8 report[49];

	make_ready(&state);
	SwitchProIncomingStartInit(&state);
	assert(SwitchProIncomingBuildInit(&state, command,
		sizeof(command), 0) == 11);
	memset(reply, 0, sizeof(reply));
	reply[0] = SWITCH_PRO_REPORT_COMMAND;
	reply[13] = 0x82;
	reply[14] = 0x02;
	assert(SwitchProIncomingHandleReport(&state, reply, sizeof(reply)) ==
		SWITCH_PRO_EVENT_ACK);
	assert(state.identity_confirmed);

	memset(report, 0, sizeof(report));
	report[0] = SWITCH_PRO_REPORT_FULL;
	report[3] = 0x08 | 0x40 | 0x80;
	report[4] = 0x02;
	report[5] = 0x02 | 0x08 | 0x40 | 0x80;
	report[6] = 0x00;
	report[7] = 0x08;
	report[8] = 0x80;
	report[9] = 0xff;
	report[10] = 0x0f;
	report[11] = 0x00;
	assert(SwitchProIncomingHandleReport(&state, report, sizeof(report)) ==
		SWITCH_PRO_EVENT_INPUT);
	assert(state.input_valid);
	assert(state.input.left_x == 0 && state.input.left_y == 0);
	assert(state.input.right_x == 127 && state.input.right_y == 127);
	assert(state.input.buttons & SWITCH_PRO_BTN_A);
	assert(state.input.buttons & SWITCH_PRO_BTN_R);
	assert(state.input.buttons & SWITCH_PRO_BTN_ZR);
	assert(state.input.buttons & SWITCH_PRO_BTN_PLUS);
	assert(state.input.buttons & SWITCH_PRO_BTN_UP);
	assert(state.input.buttons & SWITCH_PRO_BTN_LEFT);
	assert(state.input.buttons & SWITCH_PRO_BTN_L);
	assert(state.input.buttons & SWITCH_PRO_BTN_ZL);
}

int main(void)
{
	test_generic_channel_requires_valid_input();
	test_pairing_record();
	test_registration_is_independent_and_idempotent();
	test_transport_both_channel_orders();
	test_encrypted_reconnect_survives_redundant_auth_failure();
	test_security_after_channels_also_finalizes();
	test_encryption_failure_blocks_hid();
	test_full_left_y_gamecube_direction();
	test_exact_linux_init_sequence();
	test_player_led_channel_mapping_and_updates();
	test_init_led_waits_for_channel_then_resumes();
	test_player_led_retry_is_bounded_and_reconnect_resets();
	test_init_retries_and_ack_validation();
	test_delayed_spi_ack_does_not_advance_next_read();
	test_full_report_end_to_end();
	return 0;
}
