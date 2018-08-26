/*
MajoraSaveRegs.S for Nintendont (Kernel)

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
.set	STREAMING,		0xD3026580
.set	AI_ADP_LOC,		0xD30265C0

.set	tmp,			28
.set	buf,			27
.set	loc,			26
.set	glob,			25
.set	cmpstrm,		5	#unused

#they left the stack room but didnt use it, good for us
MajoraSaveRegs:
	stw		glob,	0x10(r1)
	stw		buf,	0xC(r1)
	stw		loc,	0x8(r1)
	stw		tmp,	0x4(r1)
	lis		glob,	STREAMING@h
	lwz		tmp,	STREAMING@l(glob)
	cmpwi	cmpstrm,tmp,	0
	beq		cmpstrm,end
	lwz		loc,	AI_ADP_LOC@l(glob)
end:
	nop		#b ori
