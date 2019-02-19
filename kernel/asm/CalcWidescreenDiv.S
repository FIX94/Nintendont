/*
CalcWidescreenDiv.S for Nintendont (Kernel)

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

CalcWidescreen:
	nop		#original instruction
	stwu	%sp,	-0x20(%sp)
	stw		r30,	0x4(%sp)
	stw		r31,	0x8(%sp)
	stfd	%f31,	0xC(%sp)
	mflr	r30
	bl		GetFloatBase
	.long	0x3f400000	#3/4
GetFloatBase:
	mflr	r31
	lfs		%f31,	0(r31)
MulAspect:
	fmuls	%f3,	%f31,	%f3 #gets replaced with float we want
end:
	mtlr	r30
	lwz		r30,	0x4(%sp)
	lwz		r31,	0x8(%sp)
	lfd		%f31,	0xC(%sp)
	addi	%sp,	%sp,	0x20
	nop		#b original code
