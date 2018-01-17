/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef _WIIDRC_H_
#define _WIIDRC_H_

#ifdef __cplusplus
extern "C" {
#endif

struct WiiDRCData {
	s16 xAxisL;
	s16 xAxisR;
	s16 yAxisL;
	s16 yAxisR;
	u16 button;
	u8 battery;
	u8 extra;
};

#define WIIDRC_BUTTON_A			0x8000
#define WIIDRC_BUTTON_B			0x4000
#define WIIDRC_BUTTON_X			0x2000
#define WIIDRC_BUTTON_Y			0x1000
#define WIIDRC_BUTTON_LEFT		0x0800
#define WIIDRC_BUTTON_RIGHT		0x0400
#define WIIDRC_BUTTON_UP		0x0200
#define WIIDRC_BUTTON_DOWN		0x0100
#define WIIDRC_BUTTON_ZL		0x0080
#define WIIDRC_BUTTON_ZR		0x0040
#define WIIDRC_BUTTON_L			0x0020
#define WIIDRC_BUTTON_R			0x0010
#define WIIDRC_BUTTON_PLUS		0x0008
#define WIIDRC_BUTTON_MINUS		0x0004
#define WIIDRC_BUTTON_HOME		0x0002
#define WIIDRC_BUTTON_SYNC		0x0001

#define WIIDRC_EXTRA_BUTTON_L3		0x80
#define WIIDRC_EXTRA_BUTTON_R3		0x40
#define WIIDRC_EXTRA_BUTTON_TV		0x20
#define WIIDRC_EXTRA_OVERLAY_TV		0x10
#define WIIDRC_EXTRA_OVERLAY_POWER	0x01

bool WiiDRC_Init();
bool WiiDRC_Inited();
bool WiiDRC_Recalibrate();
bool WiiDRC_ScanPads();
bool WiiDRC_Connected();
bool WiiDRC_ShutdownRequested();
const u8 *WiiDRC_GetRawI2CAddr();
const struct WiiDRCData *WiiDRC_Data();
u32 WiiDRC_ButtonsUp();
u32 WiiDRC_ButtonsDown();
u32 WiiDRC_ButtonsHeld();
s16 WiiDRC_lStickX();
s16 WiiDRC_lStickY();
s16 WiiDRC_rStickX();
s16 WiiDRC_rStickY();

#ifdef __cplusplus
}
#endif

#endif
