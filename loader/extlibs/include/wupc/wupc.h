/****************************************************************************
 * Copyright (C) 2014 FIX94
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#ifndef _WUPC_H_
#define _WUPC_H_

#ifdef __cplusplus
extern "C" {
#endif

struct WUPCData {
	s16 xAxisL;
	s16 xAxisR;
	s16 yAxisL;
	s16 yAxisR;
	u32 button;
	u8 battery;
	u8 extra;
};

#define WUPC_EXTRA_BUTTON_RSTICK	0x01
#define WUPC_EXTRA_BUTTON_LSTICK	0x02

#define WUPC_EXTRA_CHARGING			0x04
#define WUPC_EXTRA_USBCONNECTED		0x08

void WUPC_Init();
void WUPC_Disconnect(u8 chan);
void WUPC_Shutdown();
struct WUPCData *WUPC_Data(u8 chan);
void WUPC_Rumble(u8 chan, bool rumble);
u32 WUPC_UpdateButtonStats();
u32 WUPC_ButtonsUp(u8 chan);
u32 WUPC_ButtonsDown(u8 chan);
u32 WUPC_ButtonsHeld(u8 chan);
s16 WUPC_lStickX(u8 chan);
s16 WUPC_lStickY(u8 chan);
s16 WUPC_rStickX(u8 chan);
s16 WUPC_rStickY(u8 chan);

#ifdef __cplusplus
}
#endif

#endif
