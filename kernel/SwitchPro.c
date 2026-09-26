/*
 * Copyright (C) 2026 GerwinVerkerk/Nintendont contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2.
 */
#include "SwitchPro.h"

static void clear_bytes(u8 *data, u16 size)
{
	while(size--)
		*data++ = 0;
}

static void copy_bytes(u8 *target, const u8 *source, u16 size)
{
	while(size--)
		*target++ = *source++;
}

static s16 clamp_axis(s32 value)
{
	if(value > 127)
		return 127;
	if(value < -128)
		return -128;
	return (s16)value;
}

static u16 read_be16(const u8 *data)
{
	return ((u16)data[0] << 8) | data[1];
}

static u16 switch_axis_x(const u8 *data)
{
	return data[0] | ((data[1] & 0x0F) << 8);
}

static u16 switch_axis_y(const u8 *data)
{
	return (data[1] >> 4) | (data[2] << 4);
}

static void parse_full(const u8 *report, struct SwitchProInput *input)
{
	u8 right = report[3];
	u8 shared = report[4];
	u8 left = report[5];
	u32 buttons = 0;

	input->left_x = clamp_axis(((s32)switch_axis_x(&report[6]) - 0x800) >> 4);
	input->left_y = clamp_axis(-(((s32)switch_axis_y(&report[6]) - 0x800) >> 4));
	input->right_x = clamp_axis(((s32)switch_axis_x(&report[9]) - 0x800) >> 4);
	input->right_y = clamp_axis(-(((s32)switch_axis_y(&report[9]) - 0x800) >> 4));

	if(right & 0x10) buttons |= SWITCH_PRO_BTN_A;
	if(right & 0x20) buttons |= SWITCH_PRO_BTN_B;
	if(right & 0x40) buttons |= SWITCH_PRO_BTN_X;
	if(right & 0x80) buttons |= SWITCH_PRO_BTN_Y;
	if(right & 0x02) buttons |= SWITCH_PRO_BTN_R;
	if(right & 0x01) buttons |= SWITCH_PRO_BTN_ZR;
	if(shared & 0x40) buttons |= SWITCH_PRO_BTN_PLUS;
	if(shared & 0x80) buttons |= SWITCH_PRO_BTN_MINUS;
	if(shared & 0x08) buttons |= SWITCH_PRO_BTN_HOME;
	if(left & 0x02) buttons |= SWITCH_PRO_BTN_L;
	if(left & 0x01) buttons |= SWITCH_PRO_BTN_ZL;
	if(left & 0x10) buttons |= SWITCH_PRO_BTN_LEFT;
	if(left & 0x20) buttons |= SWITCH_PRO_BTN_RIGHT;
	if(left & 0x40) buttons |= SWITCH_PRO_BTN_UP;
	if(left & 0x80) buttons |= SWITCH_PRO_BTN_DOWN;
	input->buttons = buttons;
}

static void add_dpad(u8 dpad, u32 *buttons)
{
	switch(dpad)
	{
		case 0: *buttons |= SWITCH_PRO_BTN_UP; break;
		case 1: *buttons |= SWITCH_PRO_BTN_UP | SWITCH_PRO_BTN_RIGHT; break;
		case 2: *buttons |= SWITCH_PRO_BTN_RIGHT; break;
		case 3: *buttons |= SWITCH_PRO_BTN_DOWN | SWITCH_PRO_BTN_RIGHT; break;
		case 4: *buttons |= SWITCH_PRO_BTN_DOWN; break;
		case 5: *buttons |= SWITCH_PRO_BTN_DOWN | SWITCH_PRO_BTN_LEFT; break;
		case 6: *buttons |= SWITCH_PRO_BTN_LEFT; break;
		case 7: *buttons |= SWITCH_PRO_BTN_UP | SWITCH_PRO_BTN_LEFT; break;
		default: break;
	}
}

static void parse_basic(const u8 *report, struct SwitchProInput *input)
{
	u8 primary = report[1];
	u8 shared = report[2];
	u32 buttons = 0;

	input->left_x = clamp_axis(((s32)read_be16(&report[4]) - 0x8000) >> 8);
	input->left_y = clamp_axis(((s32)read_be16(&report[6]) - 0x8000) >> 8);
	input->right_x = clamp_axis(((s32)read_be16(&report[8]) - 0x8000) >> 8);
	input->right_y = clamp_axis(((s32)read_be16(&report[10]) - 0x8000) >> 8);

	if(primary & 0x40) buttons |= SWITCH_PRO_BTN_A;
	if(primary & 0x80) buttons |= SWITCH_PRO_BTN_B;
	if(primary & 0x10) buttons |= SWITCH_PRO_BTN_X;
	if(primary & 0x20) buttons |= SWITCH_PRO_BTN_Y;
	if(primary & 0x08) buttons |= SWITCH_PRO_BTN_L;
	if(primary & 0x04) buttons |= SWITCH_PRO_BTN_R;
	if(primary & 0x02) buttons |= SWITCH_PRO_BTN_ZL;
	if(primary & 0x01) buttons |= SWITCH_PRO_BTN_ZR;
	if(shared & 0x40) buttons |= SWITCH_PRO_BTN_PLUS;
	if(shared & 0x80) buttons |= SWITCH_PRO_BTN_MINUS;
	if(shared & 0x08) buttons |= SWITCH_PRO_BTN_HOME;
	add_dpad(report[3] & 0x0F, &buttons);
	input->buttons = buttons;
}

void SwitchProReset(struct SwitchProState *state)
{
	clear_bytes((u8*)state, sizeof(*state));
	state->drop_first_basic_report = 1;
}

s32 SwitchProParseReport(struct SwitchProState *state, const u8 *report,
	u16 len, struct SwitchProInput *input)
{
	if(report == 0 || input == 0 || len == 0)
		return 0;

	if((report[0] == SWITCH_PRO_REPORT_FULL ||
		report[0] == SWITCH_PRO_REPORT_COMMAND) && len >= 13)
	{
		parse_full(report, input);
		return 1;
	}

	if(report[0] == SWITCH_PRO_REPORT_BASIC && len >= 12)
	{
		if(state != 0 && state->drop_first_basic_report)
		{
			state->drop_first_basic_report = 0;
			return 0;
		}
		parse_basic(report, input);
		return 1;
	}

	return 0;
}

u16 SwitchProBuildSubcommand(struct SwitchProState *state, u8 *report,
	u16 capacity, u8 command, const u8 *data, u8 data_len)
{
	u16 size = 11 + data_len;
	if(report == 0 || state == 0 || capacity < size)
		return 0;

	clear_bytes(report, size);
	report[0] = 0x01;
	report[1] = state->report_counter++ & 0x0F;
	report[10] = command;
	if(data_len && data != 0)
		copy_bytes(&report[11], data, data_len);
	return size;
}

u8 SwitchProDiagnosticLED(u32 phase, u8 blink_on)
{
	switch(phase)
	{
		case 1: return 0x10;
		case 2: return 0x30;
		case 3: return 0x70;
		case 4: return 0xF0;
		case 5: return blink_on ? 0x50 : 0xA0;
		case 6: return blink_on ? 0xF0 : 0x00;
		case 7: return blink_on ? 0xF0 : 0x00;
		case 8: return blink_on ? 0xF0 : 0x00;
		case 9: return blink_on ? 0x90 : 0x60;
		case 10: return blink_on ? 0x30 : 0xC0;
		default: return 0x00;
	}
}
