/* Minimal incoming-only Nintendo Switch Pro Controller support. */
#include "SwitchProIncoming.h"
#ifdef SWITCH_PRO_HOST_TEST
#include <string.h>
#else
#include "string.h"
#endif

static s16 clamp_axis(s32 value)
{
	if(value > 127)
		return 127;
	if(value < -128)
		return -128;
	return (s16)value;
}

struct SwitchProInitCommand {
	u8 command;
	u8 data_len;
	u8 data[5];
};

/* Exact hid-nintendo sequence captured from the original controller. */
static const struct SwitchProInitCommand SwitchProInitCommands[] = {
	{0x02, 0, {0, 0, 0, 0, 0}},
	{0x10, 5, {0x10, 0x80, 0x00, 0x00, 0x02}},
	{0x10, 5, {0x1b, 0x80, 0x00, 0x00, 0x02}},
	{0x10, 5, {0x12, 0x80, 0x00, 0x00, 0x09}},
	{0x10, 5, {0x1d, 0x80, 0x00, 0x00, 0x09}},
	{0x10, 5, {0x26, 0x80, 0x00, 0x00, 0x02}},
	{0x10, 5, {0x28, 0x80, 0x00, 0x00, 0x18}},
	{0x40, 1, {0x01, 0, 0, 0, 0}},
	{0x03, 1, {0x30, 0, 0, 0, 0}},
	{0x48, 1, {0x01, 0, 0, 0, 0}},
	{0x30, 1, {0x00, 0, 0, 0, 0}},
	{0x38, 5, {0x01, 0x00, 0x00, 0x11, 0x11}}
};

typedef char SwitchProInitCommandCountCheck[
	(sizeof(SwitchProInitCommands) / sizeof(SwitchProInitCommands[0]) ==
	SWITCH_PRO_INIT_COMMAND_COUNT) ? 1 : -1];

static u16 switch_axis_x(const u8 *data)
{
	return data[0] | ((data[1] & 0x0f) << 8);
}

static u16 switch_axis_y(const u8 *data)
{
	return (data[1] >> 4) | (data[2] << 4);
}

static void parse_full(struct SwitchProIncomingState *state,
	const u8 *report)
{
	u8 right = report[3];
	u8 shared = report[4];
	u8 left = report[5];
	u32 buttons = 0;

	state->input.left_x = clamp_axis(
		((s32)switch_axis_x(&report[6]) - 0x800) >> 4);
	state->input.left_y = clamp_axis(
		((s32)switch_axis_y(&report[6]) - 0x800) >> 4);
	state->input.right_x = clamp_axis(
		((s32)switch_axis_x(&report[9]) - 0x800) >> 4);
	state->input.right_y = clamp_axis(
		-(((s32)switch_axis_y(&report[9]) - 0x800) >> 4));

	if(right & 0x08) buttons |= SWITCH_PRO_BTN_A;
	if(right & 0x04) buttons |= SWITCH_PRO_BTN_B;
	if(right & 0x02) buttons |= SWITCH_PRO_BTN_X;
	if(right & 0x01) buttons |= SWITCH_PRO_BTN_Y;
	if(right & 0x40) buttons |= SWITCH_PRO_BTN_R;
	if(right & 0x80) buttons |= SWITCH_PRO_BTN_ZR;
	if(shared & 0x02) buttons |= SWITCH_PRO_BTN_PLUS;
	if(shared & 0x01) buttons |= SWITCH_PRO_BTN_MINUS;
	if(shared & 0x10) buttons |= SWITCH_PRO_BTN_HOME;
	if(left & 0x40) buttons |= SWITCH_PRO_BTN_L;
	if(left & 0x80) buttons |= SWITCH_PRO_BTN_ZL;
	if(left & 0x08) buttons |= SWITCH_PRO_BTN_LEFT;
	if(left & 0x04) buttons |= SWITCH_PRO_BTN_RIGHT;
	if(left & 0x02) buttons |= SWITCH_PRO_BTN_UP;
	if(left & 0x01) buttons |= SWITCH_PRO_BTN_DOWN;
	state->input.buttons = buttons;
}

void SwitchProIncomingReset(struct SwitchProIncomingState *state)
{
	memset(state, 0, sizeof(*state));
}

void SwitchProIncomingImported(struct SwitchProIncomingState *state)
{
	state->imported = 1;
}

u8 SwitchProIncomingNeedsListener(const struct SwitchProIncomingState *state)
{
	return state != 0 && state->imported && !state->listener_registered;
}

void SwitchProIncomingListener(struct SwitchProIncomingState *state, s32 result)
{
	if(result == 0)
		state->listener_registered = 1;
}

void SwitchProIncomingACL(struct SwitchProIncomingState *state, s32 result)
{
	state->acl_connected = result == 0;
}

void SwitchProIncomingAuthentication(struct SwitchProIncomingState *state,
	s32 result)
{
	if(result == 0)
		state->authenticated = 1;
	else if(!state->encrypted)
		state->authenticated = 0;
}

void SwitchProIncomingEncryption(struct SwitchProIncomingState *state,
	s32 result, u8 enabled)
{
	state->encrypted = result == 0 && enabled != 0;
	/* A stored-key reconnect can enable encryption without emitting a
	 * separate Authentication Complete event.  Successful encryption is
	 * therefore the authoritative proof that link-key authentication
	 * completed for this incoming connection. */
	if(state->encrypted)
		state->authenticated = 1;
}

void SwitchProIncomingChannels(struct SwitchProIncomingState *state,
	u8 control_open, u8 interrupt_open)
{
	state->control_open = control_open != 0;
	state->interrupt_open = interrupt_open != 0;
}

void SwitchProIncomingTransport(struct SwitchProIncomingState *state,
	u8 transport_ready)
{
	if(state == 0)
		return;
	state->transport_ready = transport_ready != 0;
}

u8 SwitchProIncomingReady(const struct SwitchProIncomingState *state)
{
	return state != 0 && state->imported && state->listener_registered &&
		state->acl_connected && state->authenticated && state->encrypted &&
		state->control_open && state->interrupt_open;
}

u8 SwitchProIncomingNeedsFinalize(const struct SwitchProIncomingState *state)
{
	return SwitchProIncomingReady(state) && state->transport_ready &&
		!state->finalized && !state->init_failed;
}

void SwitchProIncomingFinalized(struct SwitchProIncomingState *state)
{
	if(state != 0 && SwitchProIncomingReady(state))
		state->finalized = 1;
}

void SwitchProIncomingStartInit(struct SwitchProIncomingState *state)
{
	if(state == 0 || !state->finalized || state->init_started)
		return;
	state->init_started = 1;
	state->init_index = 0;
	state->init_retries = 0;
	state->awaiting_ack = 0;
	state->report_counter = 0;
}

u8 SwitchProIncomingPlayerLedMask(u8 channel)
{
	/* Nintendo's player patterns are cumulative: player N lights the first
	 * N indicators.  Keep this protocol mapping explicit; Bloopair's direct
	 * forwarding of Wii LED bits is not a player-number mapping. */
	static const u8 masks[4] = {0x01, 0x03, 0x07, 0x0f};
	return channel < 4 ? masks[channel] : 0;
}

void SwitchProIncomingSetChannel(struct SwitchProIncomingState *state,
	u8 channel)
{
	u8 mask;
	if(state == 0)
		return;
	mask = SwitchProIncomingPlayerLedMask(channel);
	if(mask != 0)
	{
		if(mask != state->desired_led_mask)
			state->led_failed = 0;
		state->desired_led_mask = mask;
	}
}

u8 SwitchProIncomingNeedsLedUpdate(
	const struct SwitchProIncomingState *state)
{
	return state != 0 && state->init_complete &&
		state->desired_led_mask != 0 && !state->led_awaiting_ack &&
		!state->led_failed &&
		state->desired_led_mask != state->applied_led_mask;
}

u16 SwitchProIncomingBuildInit(struct SwitchProIncomingState *state,
	u8 *report, u16 capacity, u8 retry)
{
	const struct SwitchProInitCommand *entry;
	u16 size;

	if(state == 0 || report == 0 || !state->init_started ||
		state->init_complete || state->init_failed ||
		state->init_index >= SWITCH_PRO_INIT_COMMAND_COUNT)
		return 0;
	if((retry && !state->awaiting_ack) ||
		(!retry && state->awaiting_ack))
		return 0;
	entry = &SwitchProInitCommands[state->init_index];
	if(entry->command == 0x30 && !retry && state->desired_led_mask == 0)
		return 0;
	size = 11 + entry->data_len;
	if(capacity < size)
		return 0;
	if(retry)
	{
		if(state->init_retries >= SWITCH_PRO_INIT_RETRY_MAX)
		{
			state->init_failed = 1;
			return 0;
		}
		state->init_retries++;
	}
	else
		state->init_retries = 0;
	memset(report, 0, size);
	report[0] = 0x01;
	report[1] = state->report_counter++ & 0x0f;
	report[10] = entry->command;
	if(entry->data_len)
		memcpy(&report[11], entry->data, entry->data_len);
	if(entry->command == 0x30)
	{
		if(retry)
			report[11] = state->sent_led_mask;
		else
		{
			state->sent_led_mask = state->desired_led_mask;
			report[11] = state->sent_led_mask;
		}
	}
	state->awaiting_ack = 1;
	return size;
}

u16 SwitchProIncomingBuildLedUpdate(struct SwitchProIncomingState *state,
	u8 *report, u16 capacity, u8 retry)
{
	if(state == 0 || report == 0 || capacity < 12 ||
		!state->init_complete || state->desired_led_mask == 0)
		return 0;
	if((retry && !state->led_awaiting_ack) ||
		(!retry && (state->led_awaiting_ack ||
		state->desired_led_mask == state->applied_led_mask)))
		return 0;
	if(retry)
	{
		if(state->led_retries >= SWITCH_PRO_INIT_RETRY_MAX)
		{
			state->led_awaiting_ack = 0;
			state->led_failed = 1;
			return 0;
		}
		state->led_retries++;
	}
	else
	{
		state->led_retries = 0;
		state->sent_led_mask = state->desired_led_mask;
	}
	memset(report, 0, 12);
	report[0] = 0x01;
	report[1] = state->report_counter++ & 0x0f;
	report[10] = 0x30;
	report[11] = state->sent_led_mask;
	state->led_awaiting_ack = 1;
	return 12;
}

s32 SwitchProIncomingHandleReport(struct SwitchProIncomingState *state,
	const u8 *report, u16 len)
{
	const struct SwitchProInitCommand *entry;

	if(state == 0 || report == 0 || len == 0)
		return SWITCH_PRO_EVENT_NONE;
	if(report[0] == SWITCH_PRO_REPORT_FULL && len >= 12)
	{
		parse_full(state, report);
		state->input_valid = state->identity_confirmed;
		return state->input_valid ? SWITCH_PRO_EVENT_INPUT :
			SWITCH_PRO_EVENT_NONE;
	}
	if(report[0] != SWITCH_PRO_REPORT_COMMAND || len < 15)
		return SWITCH_PRO_EVENT_NONE;

	if(state->led_awaiting_ack && report[14] == 0x30)
	{
		if((report[13] & 0x80) == 0)
			return SWITCH_PRO_EVENT_NEGATIVE_ACK;
		state->led_awaiting_ack = 0;
		state->led_retries = 0;
		state->applied_led_mask = state->sent_led_mask;
		return SWITCH_PRO_EVENT_ACK;
	}
	if(!state->init_started || state->init_complete || state->init_failed ||
		!state->awaiting_ack ||
		state->init_index >= SWITCH_PRO_INIT_COMMAND_COUNT)
		return SWITCH_PRO_EVENT_NONE;
	entry = &SwitchProInitCommands[state->init_index];
	if(report[14] != entry->command)
		return SWITCH_PRO_EVENT_NONE;
	/* Consecutive SPI reads all use subcommand 0x10.  Match the echoed
	 * address and size as well so a delayed retry response cannot advance
	 * the following read. */
	if(entry->command == 0x10 &&
		(len < 20 || memcmp(&report[15], entry->data, 5) != 0))
		return SWITCH_PRO_EVENT_NONE;
	if((report[13] & 0x80) == 0)
		return SWITCH_PRO_EVENT_NEGATIVE_ACK;

	state->awaiting_ack = 0;
	state->init_retries = 0;
	if(entry->command == 0x30)
		state->applied_led_mask = state->sent_led_mask;
	if(state->init_index == 0)
		state->identity_confirmed = 1;
	state->init_index++;
	if(state->init_index >= SWITCH_PRO_INIT_COMMAND_COUNT)
		state->init_complete = 1;
	return SWITCH_PRO_EVENT_ACK;
}
