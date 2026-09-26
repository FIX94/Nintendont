#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "SwitchPro.h"

static void pack_axis(u8 *data, u16 x, u16 y)
{
	data[0] = x & 0xFF;
	data[1] = ((x >> 8) & 0x0F) | ((y & 0x0F) << 4);
	data[2] = (y >> 4) & 0xFF;
}

static void test_full_report(void)
{
	struct SwitchProState state;
	struct SwitchProInput input;
	u8 report[13];

	memset(report, 0, sizeof(report));
	SwitchProReset(&state);
	report[0] = SWITCH_PRO_REPORT_FULL;
	report[3] = 0x10 | 0x20 | 0x40 | 0x80 | 0x02 | 0x01;
	report[4] = 0x40 | 0x80 | 0x08;
	report[5] = 0x02 | 0x01 | 0x10 | 0x20 | 0x40 | 0x80;
	pack_axis(&report[6], 0xFFF, 0x000);
	pack_axis(&report[9], 0x000, 0xFFF);

	assert(SwitchProParseReport(&state, report, sizeof(report), &input) == 1);
	assert(input.left_x == 127);
	assert(input.left_y == 127);
	assert(input.right_x == -128);
	assert(input.right_y == -127);
	assert(input.buttons & SWITCH_PRO_BTN_A);
	assert(input.buttons & SWITCH_PRO_BTN_B);
	assert(input.buttons & SWITCH_PRO_BTN_X);
	assert(input.buttons & SWITCH_PRO_BTN_Y);
	assert(input.buttons & SWITCH_PRO_BTN_R);
	assert(input.buttons & SWITCH_PRO_BTN_ZR);
	assert(input.buttons & SWITCH_PRO_BTN_PLUS);
	assert(input.buttons & SWITCH_PRO_BTN_MINUS);
	assert(input.buttons & SWITCH_PRO_BTN_HOME);
	assert(input.buttons & SWITCH_PRO_BTN_L);
	assert(input.buttons & SWITCH_PRO_BTN_ZL);
	assert(input.buttons & SWITCH_PRO_BTN_LEFT);
	assert(input.buttons & SWITCH_PRO_BTN_RIGHT);
	assert(input.buttons & SWITCH_PRO_BTN_UP);
	assert(input.buttons & SWITCH_PRO_BTN_DOWN);
}

static void test_basic_report_and_first_packet_drop(void)
{
	struct SwitchProState state;
	struct SwitchProInput input;
	u8 report[12] = {
		SWITCH_PRO_REPORT_BASIC, 0x40 | 0x08 | 0x02,
		0x80, 0x01,
		0x80, 0x00, 0x80, 0x00,
		0xFF, 0xFF, 0x00, 0x00
	};

	SwitchProReset(&state);
	assert(SwitchProParseReport(&state, report, sizeof(report), &input) == 0);
	assert(SwitchProParseReport(&state, report, sizeof(report), &input) == 1);
	assert(input.left_x == 0 && input.left_y == 0);
	assert(input.right_x == 127 && input.right_y == -128);
	assert(input.buttons & SWITCH_PRO_BTN_A);
	assert(input.buttons & SWITCH_PRO_BTN_L);
	assert(input.buttons & SWITCH_PRO_BTN_ZL);
	assert(input.buttons & SWITCH_PRO_BTN_MINUS);
	assert(input.buttons & SWITCH_PRO_BTN_UP);
	assert(input.buttons & SWITCH_PRO_BTN_RIGHT);
}

static void test_subcommand(void)
{
	struct SwitchProState state;
	u8 report[16];
	u8 mode = SWITCH_PRO_REPORT_FULL;
	u16 len;

	SwitchProReset(&state);
	len = SwitchProBuildSubcommand(&state, report, sizeof(report),
		SWITCH_PRO_SUBCMD_REPORT_MODE, &mode, 1);
	assert(len == 12);
	assert(report[0] == 0x01);
	assert(report[1] == 0x00);
	assert(report[10] == SWITCH_PRO_SUBCMD_REPORT_MODE);
	assert(report[11] == SWITCH_PRO_REPORT_FULL);

	len = SwitchProBuildSubcommand(&state, report, sizeof(report),
		SWITCH_PRO_SUBCMD_DEVICE_INFO, NULL, 0);
	assert(len == 11);
	assert(report[1] == 0x01);
	assert(report[10] == SWITCH_PRO_SUBCMD_DEVICE_INFO);
}

static void test_diagnostic_leds(void)
{
	assert(SwitchProDiagnosticLED(0, 0) == 0x00);
	assert(SwitchProDiagnosticLED(1, 0) == 0x10);
	assert(SwitchProDiagnosticLED(2, 0) == 0x30);
	assert(SwitchProDiagnosticLED(3, 0) == 0x70);
	assert(SwitchProDiagnosticLED(4, 0) == 0xF0);
	assert(SwitchProDiagnosticLED(5, 0) == 0xA0);
	assert(SwitchProDiagnosticLED(5, 1) == 0x50);
	assert(SwitchProDiagnosticLED(6, 0) == 0x00);
	assert(SwitchProDiagnosticLED(6, 1) == 0xF0);
	assert(SwitchProDiagnosticLED(7, 0) == 0x00);
	assert(SwitchProDiagnosticLED(7, 1) == 0xF0);
	assert(SwitchProDiagnosticLED(8, 0) == 0x00);
	assert(SwitchProDiagnosticLED(8, 1) == 0xF0);
}

int main(void)
{
	test_full_report();
	test_basic_report_and_first_packet_drop();
	test_subcommand();
	test_diagnostic_leds();
	puts("switch_pro tests: ok");
	return 0;
}
