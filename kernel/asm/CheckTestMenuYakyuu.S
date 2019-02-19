/*
CheckTestMenuYakyuu.S for Nintendont (Kernel)

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
.include "constants.inc"

#r6 is unused at this point

CheckTestMenu:
	#original instruction
	extrwi	r0,	r0,	8,19

	lis		r6,	IN_TESTMENU@h
	cmpwi	r0,	0
	bne		DoCheck
	b		end		#No Test Menu, just exit
DoCheck:	#Test Menu requested, make sure it wasnt loaded
	lwz		r0,	IN_TESTMENU@l(r6)
	cmpwi	r0,	0
	bne		Disallow
Allow:	#Not loaded previously, enter test menu
	li		r0,	1
	b		end
Disallow:	#Test Menu previously loaded so disallow
	li		r0,	0
end:	#Always save current status for next time
	stw		r0,	IN_TESTMENU@l(r6)
	blr
