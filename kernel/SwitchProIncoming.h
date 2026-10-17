#ifndef _SWITCH_PRO_INCOMING_H_
#define _SWITCH_PRO_INCOMING_H_

#ifdef SWITCH_PRO_HOST_TEST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
#else
#include "global.h"
#endif

#define SWITCH_PRO_REPORT_BASIC 0x3f
#define SWITCH_PRO_REPORT_COMMAND 0x21
#define SWITCH_PRO_REPORT_FULL 0x30
#define SWITCH_PRO_CANONICAL_CONTROLLER 0x00000001u

#define SWITCH_PRO_INIT_COMMAND_COUNT 12
#define SWITCH_PRO_INIT_RETRY_MAX 3

#define SWITCH_PRO_EVENT_NONE 0
#define SWITCH_PRO_EVENT_ACK 1
#define SWITCH_PRO_EVENT_INPUT 2
#define SWITCH_PRO_EVENT_NEGATIVE_ACK 3

#define SWITCH_PRO_BTN_UP       0x0001
#define SWITCH_PRO_BTN_LEFT     0x0002
#define SWITCH_PRO_BTN_ZR       0x0004
#define SWITCH_PRO_BTN_X        0x0008
#define SWITCH_PRO_BTN_A        0x0010
#define SWITCH_PRO_BTN_Y        0x0020
#define SWITCH_PRO_BTN_B        0x0040
#define SWITCH_PRO_BTN_ZL       0x0080
#define SWITCH_PRO_BTN_R        0x0200
#define SWITCH_PRO_BTN_PLUS     0x0400
#define SWITCH_PRO_BTN_HOME     0x0800
#define SWITCH_PRO_BTN_MINUS    0x1000
#define SWITCH_PRO_BTN_L        0x2000
#define SWITCH_PRO_BTN_DOWN     0x4000
#define SWITCH_PRO_BTN_RIGHT    0x8000

struct SwitchProIncomingInput {
	s16 left_x;
	s16 left_y;
	s16 right_x;
	s16 right_y;
	u32 buttons;
};

struct SwitchProIncomingState {
	u8 imported;
	u8 listener_registered;
	u8 acl_connected;
	u8 authenticated;
	u8 encrypted;
	u8 control_open;
	u8 interrupt_open;
	u8 transport_ready;
	u8 finalized;
	u8 init_started;
	u8 init_complete;
	u8 init_failed;
	u8 init_index;
	u8 init_retries;
	u8 awaiting_ack;
	u8 report_counter;
	u8 identity_confirmed;
	u8 input_valid;
	u8 desired_led_mask;
	u8 sent_led_mask;
	u8 applied_led_mask;
	u8 led_awaiting_ack;
	u8 led_retries;
	u8 led_failed;
	struct SwitchProIncomingInput input;
};

void SwitchProIncomingReset(struct SwitchProIncomingState *state);
void SwitchProIncomingImported(struct SwitchProIncomingState *state);
u8 SwitchProIncomingNeedsListener(const struct SwitchProIncomingState *state);
void SwitchProIncomingListener(struct SwitchProIncomingState *state, s32 result);
void SwitchProIncomingACL(struct SwitchProIncomingState *state, s32 result);
void SwitchProIncomingAuthentication(struct SwitchProIncomingState *state,
	s32 result);
void SwitchProIncomingEncryption(struct SwitchProIncomingState *state,
	s32 result, u8 enabled);
void SwitchProIncomingChannels(struct SwitchProIncomingState *state,
	u8 control_open, u8 interrupt_open);
void SwitchProIncomingTransport(struct SwitchProIncomingState *state,
	u8 transport_ready);
u8 SwitchProIncomingReady(const struct SwitchProIncomingState *state);
u8 SwitchProIncomingNeedsFinalize(const struct SwitchProIncomingState *state);
void SwitchProIncomingFinalized(struct SwitchProIncomingState *state);
void SwitchProIncomingStartInit(struct SwitchProIncomingState *state);
u8 SwitchProIncomingPlayerLedMask(u8 channel);
void SwitchProIncomingSetChannel(struct SwitchProIncomingState *state,
	u8 channel);
u8 SwitchProIncomingNeedsLedUpdate(
	const struct SwitchProIncomingState *state);
u16 SwitchProIncomingBuildInit(struct SwitchProIncomingState *state,
	u8 *report, u16 capacity, u8 retry);
u16 SwitchProIncomingBuildLedUpdate(struct SwitchProIncomingState *state,
	u8 *report, u16 capacity, u8 retry);
s32 SwitchProIncomingHandleReport(struct SwitchProIncomingState *state,
	const u8 *report, u16 len);
#endif
