/*
BT.h for Nintendont (Kernel)

Copyright (C) 2014 FIX94

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef _BT_H_
#define _BT_H_

#include "lwbt/bte.h"

void BTInit(void);
void BTUpdateRegisters(void);

struct BTPadStat {
	u32 controller;
	u32 timeout;
	u32 transfertype;
	u32 transferstate;
	u32 channel;
	u32 rumble;
	u32 rumbletime;
	s16 xAxisLmid;
	s16 xAxisRmid;
	s16 yAxisLmid;
	s16 yAxisRmid;
	struct bte_pcb *sock;
	struct bd_addr bdaddr;
} ALIGNED(32);

struct BTPadCont {
	u32 used;
	s16 xAxisL;
	s16 xAxisR;
	s16 yAxisL;
	s16 yAxisR;
	u32 button;
	u8 triggerL;
	u8 triggerR;
	s16 xAccel;
	s16 yAccel;
	s16 zAccel;
} ALIGNED(32);

#define BT_DPAD_UP              0x0001
#define BT_DPAD_LEFT            0x0002
#define BT_TRIGGER_ZR           0x0004
#define BT_BUTTON_X             0x0008
#define BT_BUTTON_A             0x0010
#define BT_BUTTON_Y             0x0020
#define BT_BUTTON_B             0x0040
#define BT_TRIGGER_ZL           0x0080
#define BT_TRIGGER_R            0x0200
#define BT_BUTTON_START         0x0400
#define BT_BUTTON_HOME          0x0800
#define BT_BUTTON_SELECT        0x1000
#define BT_TRIGGER_L            0x2000
#define BT_DPAD_DOWN            0x4000
#define BT_DPAD_RIGHT           0x8000


#define WM_BUTTON_TWO			0x0001
#define WM_BUTTON_ONE			0x0002
#define WM_BUTTON_B				0x0004
#define WM_BUTTON_A				0x0008
#define WM_BUTTON_MINUS			0x0010
#define NUN_BUTTON_Z			0x0020 
#define NUN_BUTTON_C			0x0040
#define WM_BUTTON_HOME			0x0080
#define WM_BUTTON_LEFT			0x0100
#define WM_BUTTON_RIGHT			0x0200
#define WM_BUTTON_DOWN			0x0400
#define WM_BUTTON_UP			0x0800
#define WM_BUTTON_PLUS			0x1000

/* From LibOGC conf.h */

#define CONF_PAD_MAX_REGISTERED 10
#define CONF_PAD_MAX_ACTIVE 4

typedef struct _conf_pad_device conf_pad_device;

struct _conf_pad_device {
	u8 bdaddr[6];
	char name[0x40];
} __attribute__((packed));

typedef struct _conf_pads conf_pads;

struct _conf_pads {
	u8 num_registered;
	conf_pad_device registered[CONF_PAD_MAX_REGISTERED];
	conf_pad_device active[CONF_PAD_MAX_ACTIVE];
	conf_pad_device balance_board;
	conf_pad_device unknown;
} __attribute__((packed));

#endif
