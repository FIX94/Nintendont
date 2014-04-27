/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

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
#include "FPad.h"

static u32 WPAD_Pressed;
static u32 PAD_Pressed;
static s8  PAD_Stick_Y;
static s8  PAD_Stick_X;
static bool SLock;
static u32 SpeedX;

#define DELAY_START	900
#define DELAY_STEP	100
#define DELAY_STOP	100

void FPAD_Init( void )
{
	PAD_Init();
	WPAD_Init();

	WPAD_Pressed = 0;
	PAD_Pressed = 0;
}
void FPAD_Update( void )
{
	if (WPAD_ScanPads() > WPAD_ERR_NONE) {
		WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		WPAD_Pressed |= WPAD_ButtonsHeld(0) | WPAD_ButtonsHeld(1) | WPAD_ButtonsHeld(2) | WPAD_ButtonsHeld(3);
	} else {
		// No Wii remotes are connected
		WPAD_Pressed = 0;
	}
	
	if (PAD_ScanPads() > PAD_ERR_NONE) {
		PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
		PAD_Pressed  |= PAD_ButtonsHeld(0) | PAD_ButtonsHeld(1) | PAD_ButtonsHeld(2) | PAD_ButtonsHeld(3);
		PAD_Stick_Y	 = PAD_StickY(0) | PAD_StickY(1) | PAD_StickY(2) | PAD_StickY(3);
		PAD_Stick_X	 = PAD_StickX(0) | PAD_StickX(1) | PAD_StickX(2) | PAD_StickX(3);
	} else {
		// No GC controllers are connected
		PAD_Pressed = 0;
		PAD_Stick_Y = 0;
		PAD_Stick_X = 0;
	}
		
	if( WPAD_Pressed == 0 && PAD_Pressed == 0 && ( PAD_Stick_Y < 25 && PAD_Stick_Y > -25 )  && ( PAD_Stick_X < 25 && PAD_Stick_X > -25 ) )
	{
		SLock = false;
		SpeedX= DELAY_START;
	}
}
bool FPAD_Up( bool ILock )
{
	if( !ILock && SLock ) return false;

	if((WPAD_Pressed & (WPAD_BUTTON_UP|WPAD_CLASSIC_BUTTON_UP)) || (PAD_Pressed & PAD_BUTTON_UP) || (PAD_Stick_Y > 30))
	{
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Down( bool ILock )
{
	if( !ILock && SLock ) return false;

	if( (WPAD_Pressed & (WPAD_BUTTON_DOWN|WPAD_CLASSIC_BUTTON_DOWN)) || (PAD_Pressed & PAD_BUTTON_DOWN) || (PAD_Stick_Y < -30))
	{
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Left( bool ILock )
{
	if( !ILock && SLock ) return false;

	if((WPAD_Pressed & (WPAD_BUTTON_LEFT|WPAD_CLASSIC_BUTTON_LEFT)) || (PAD_Pressed & PAD_BUTTON_LEFT) || (PAD_Stick_X < -30))
	{
		SLock = true;
		return true;
	}
	return false;
}
bool FPAD_Right( bool ILock )
{
	if( !ILock && SLock ) return false;

	if( (WPAD_Pressed & (WPAD_BUTTON_RIGHT|WPAD_CLASSIC_BUTTON_RIGHT)) || (PAD_Pressed & PAD_BUTTON_RIGHT) || ( PAD_Stick_X > 30 ))
	{
		SLock = true;
		return true;
	}
	return false;
}
bool FPAD_OK( bool ILock )
{
	if( !ILock && SLock ) return false;

	if( (WPAD_Pressed & (WPAD_BUTTON_A|WPAD_CLASSIC_BUTTON_A)) || ( PAD_Pressed & PAD_BUTTON_A ) )
	{
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_X( bool ILock )
{
	if( !ILock && SLock ) return false;

	if( (WPAD_Pressed & (WPAD_BUTTON_1|WPAD_CLASSIC_BUTTON_X)) || ( PAD_Pressed & PAD_BUTTON_X ))
	{
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Cancel( bool ILock )
{
	if( !ILock && SLock ) return false;

	if( (WPAD_Pressed & (WPAD_BUTTON_B|WPAD_CLASSIC_BUTTON_B)) || ( PAD_Pressed & PAD_BUTTON_B ))
	{
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Start( bool ILock )
{
	if( !ILock && SLock ) return false;

	if( WPAD_Pressed & (WPAD_BUTTON_HOME|WPAD_CLASSIC_BUTTON_HOME) || ( PAD_Pressed & PAD_BUTTON_START ))
	{
		SLock = true;
		return true;
	}
	return false;
}
