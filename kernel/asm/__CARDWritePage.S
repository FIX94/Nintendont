#include <asm.h>

# issue read command
#
#	r3	CARDBlockEntry
#	r4	cb

__CARDWritePage:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x60(%sp)
	stmw    %r19, 0x2C(%sp)
	
	lis     %r7,	0xC000
	lis     %r0,	0x2100
	
	stw     %r0,	0x2F60(%r7)

	lis		%r3,	0x803F
	addi	%r3,	%r3,	0x1300

	lwz		%r0,	0xB4(%r3)		# ptr
	stw     %r0,	0x2F64(%r7)

	lwz		%r0,	0xB0(%r3)		# Offset
	stw     %r0,	0x2F68(%r7)
	
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

cb:	
	cmpwi	%r4,	0
	beq		skip_cb
	mtctr	%r4
	li      %r3,	0
	li      %r4,	0
	bctrl

skip_cb:

	li      %r3,	0

	lmw     %r19, 0x2C(%sp)
	lwz     %r0,	0x64(%sp)
	addi    %sp, %sp, 0x60
	mtlr    %r0
	blr
