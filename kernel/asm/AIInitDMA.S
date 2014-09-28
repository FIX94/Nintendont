/*
AIInitDMA.S for Nintendont (Kernel)

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
#include <asm.h>

.set	AI_MEM_BASE,	0xCD80682C

AIInitDMA:
	rlwinm	%r0,	%r30,	0,		7,		26 # 0x01ffffe0

	lis		%r5,	AI_MEM_BASE@h
	stwu	%r0,	AI_MEM_BASE@l(%r5)
	stw		%r31,	4(%r5)

	lis		%r4,	0xCC00
	blr
