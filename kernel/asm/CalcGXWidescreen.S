/*
CalcGXWidescreen.S for Nintendont (Kernel)

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

CalcGXWidescreen:
	stwu	%sp,	-0x20(%sp)
	stw		r30,	0x4(%sp)
	stw		r4,	0x8(%sp)
	stfd	%f31,	0xC(%sp)
	mflr	r30
	lfs		%f0,	4(r31)
	lfs		%f31,	0x18(r31)
	fdivs	%f0,	%f31,	%f0
	bl		GetFloatBase
	.long	0x3faaaaab	#4/3
GetFloatBase:
	mflr	r4
	lfs		%f31,	0(r4)
MulAspect:
	fmuls	%f0,	%f31,	%f0
end:
	lfs		%f31,	0x18(r31)
	fdivs	%f0,	%f31,	%f0
	stfs	%f0,	4(r31)
	mtlr	r30
	lwz		r30,	0x4(%sp)
	lwz		r4,	0x8(%sp)
	lfd		%f31,	0xC(%sp)
	addi	%sp,	%sp,	0x20
	nop		#original instruction
	nop		#b original code
