/*
DVDSendCMDEncrypted.S for Nintendont (Kernel)

Copyright (C) 2016 FIX94

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
.include "constants.inc"

DVDSendCMDEncrypted:
	lis %r6, DI_BASE@h
	ori %r6, %r6, DI_BASE@l
	rotrwi %r3, %r3, 8 #GC Norm
	stw %r3, 0x08(%r6) #DI_CMD_0
	stw %r4, 0x0C(%r6) #DI_CMD_1
	stw %r5, 0x10(%r6) #DI_CMD_2
	blr
