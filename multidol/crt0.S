# Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

.extern _main
	.globl _start
_start:

	# Setup stack.
	lis %sp, __stack_top@ha ; addi %sp,%sp,__stack_top@l ; li %r0,0 ; stwu %r0,-0x40(%sp)

	# Clear BSS.
	lis %r3,__bss_start@ha ; addi %r3,%r3,__bss_start@l
	li %r4,0
	lis %r5,__bss_end@ha ; addi %r5,%r5,__bss_end@l ; sub %r5,%r5,%r3
	bl _memset

	# Go!
	bl _main

	#goto entrypoint
	mtlr %r3
	blr
