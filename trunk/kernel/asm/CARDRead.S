#include <asm.h>

#
#	r3	file *
#	r4	buffer
#	r5	length
#	r6	offset
#	r7	cb

CardRead:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x28(%sp)
	stmw    %r27, 0x14(%sp)

#Update fileinfo
	stw     %r5,	0x0C(%r3)
	stw     %r6,	0x08(%r3)

#send cmd to DM

	mr		%r12,	%r7

	lis     %r7,	0xC000
	lis     %r0,	0xC900
	
	stw     %r0,	0x2F60(%r7)
	
	stw     %r4,	0x2F64(%r7)
	stw     %r5,	0x2F68(%r7)	

	stw     %r6,	0x2F6C(%r7)	

	lwz		%r0,	0x04(%r3)
	stw     %r0,	0x2F70(%r7)	
	
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
	lis		%r6,	0xD201

	lwz		%r0,	0(%r6)
	stw		%r0,	0(%r4)
	addic.	%r5,	%r5,	-4

memcpy:
	lwzu	%r0,	4(%r6)
	stwu	%r0,	4(%r4)
	addic.	%r5,	%r5,	-4
	bne		memcpy

	cmpwi	%r12,	0
	beq		skip_cb
	mtlr	%r12
	li      %r3,	0
	li      %r4,	0
	blrl

skip_cb:	
	li      %r3,	0
	mr		%r4,	%r3

	lmw		%r27,	0x14(%sp)
	lwz		%r0,	0x2C(%sp)
	addi	%sp,	%sp,	0x28
	mtlr	%r0
	blr
