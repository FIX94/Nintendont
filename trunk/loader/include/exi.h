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

#ifndef __EXI_H_  // __EXI_H__ is defined in ogc/exi.h :(
#define __EXI_H_

#include <stdarg.h>
#include <string.h>
#include <ogc/usbgecko.h>
#include "global.h"

void CheckForGecko(void);
void closeLog(void);

#ifdef DEBUG
int gprintf(const char *str, ...);
#else
#define gprintf(...) 0
#endif

#endif
