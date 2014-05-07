#include <asm.h>

#
#	r3	chan
#	r4	FileNo
#	r5	Stat
#	r6	cb

CARDGetStats:

	cmpwi	%r3,	0
	beq		DoCode
	li		%r3,	0
	li		%r4,	-3
	blr

DoCode:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x10(%sp)	
			
#send cmd to nintendont
			
	lis     %r7,	0xC000
	lis     %r0,	0xC300
	
	stw     %r0,	0x2F60(%r7)
	stw     %r4,	0x2F64(%r7)
	stw     %r5,	0x2F68(%r7)	
	
	li      %r0,	3
	stw     %r0,	0x2F78(%r7)
	
ready_loop:
	lwz		%r0,	0x2F78(%r7)
	cmpwi	%r0,	3
	beq		ready_loop

wait_loop:
	lwz		%r0,	0x2F9C(%r7)
	andi.	%r0,	%r0,	0x14
	cmpwi	%r0,	0
	beq		wait_loop

memcpy_pre:	
	li		%r6,	0x6C
	lis		%r4,	0xD301

	lwz		%r0,	0(%r4)
	stw		%r0,	0(%r5)
	addic.	%r6,	%r6,	-4

memcpy:
	lwzu	%r0,	4(%r4)
	stwu	%r0,	4(%r5)
	addic.	%r6,	%r6,	-4
	bne		memcpy

	lwz     %r3,	0x2F94(%r7)
	mr		%r4,	%r3

	lwz     %r0, 0x14(%sp)
	addi    %sp, %sp, 0x10
	mtlr    %r0
	blr
