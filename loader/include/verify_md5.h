/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U
MD5sum verifier.

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

#ifndef __VERIFY_MD5_H__
#define __VERIFY_MD5_H__

#include "menu.h"

/**
 * Verify a game's MD5.
 * @param gameinfo Game to verify.
 */
void VerifyMD5(const gameinfo *gi);

#endif
