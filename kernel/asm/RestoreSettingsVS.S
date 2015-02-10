/*
RestoreSettingsVS.S for Nintendont (Kernel)

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

.set VS4_BACKUP,	0xD3002780
.set VS4_DEST1,		0x804768D8
.set VS4_DEST2,		0x8069E4E0

RestoreSettingsVS:
	lis		%r3,	VS4_BACKUP@h
	ori		%r3,	%r3,	VS4_BACKUP@l
	lis		%r4,	VS4_DEST1@h
	ori		%r4,	%r4,	VS4_DEST1@l
	lis		%r5,	VS4_DEST2@h
	ori		%r5,	%r5,	VS4_DEST2@l
	li		%r6,	0x2B
memcpy:
	subic.	%r6,	%r6,	1
	lbzx	%r0,	%r3,	%r6
	stbx	%r0,	%r4,	%r6
	stbx	%r0,	%r5,	%r6
	bne		memcpy
	blr
