/*
RestoreSettingsYAKRVB.S for Nintendont (Kernel)

Copyright (C) 2015 FIX94

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
#include <asm.h>

.set YAK_BACKUP,	(0xD3003500+0xE8)
.set YAK_DEST,		(0x803D3D0C+0xE8)

RestoreSettingsYAKRVB:
	lis		%r3,	YAK_BACKUP@h
	ori		%r3,	%r3,	YAK_BACKUP@l
	lis		%r4,	YAK_DEST@h
	ori		%r4,	%r4,	YAK_DEST@l
	li		%r5,	(0xF5-0xE8)
memcpy:
	subic.	%r5,	%r5,	1
	lbzx	%r0,	%r3,	%r5
	stbx	%r0,	%r4,	%r5
	bne		memcpy
	blr
