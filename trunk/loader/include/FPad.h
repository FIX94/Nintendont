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
#ifndef __FPAD_H__
#define __FPAD_H__

#include "global.h"
#include <wiiuse/wpad.h>
#include <ogc/pad.h>

void FPAD_Init( void );
void FPAD_Update( void );

bool FPAD_Up( bool ILock );
bool FPAD_Down( bool ILock );
bool FPAD_Left( bool ILock );
bool FPAD_Right( bool ILock );
bool FPAD_OK( bool ILock );
bool FPAD_Cancel( bool ILock );
bool FPAD_Start( bool ILock );
bool FPAD_X( bool ILock );
bool FPAD_Y( bool ILock );

#endif
