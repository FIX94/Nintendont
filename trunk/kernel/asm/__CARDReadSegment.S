#include <asm.h>

# issue read command
#
#	r3	CARDBlockEntry
#	r4	cb

__CARDReadSegment:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x60(%sp)
	stmw    %r19, 0x2C(%sp)

#	
# CB

#r4	803F13C8->80319894	BlockReadCallback

	lis		%r5,	0x803F
	stw		%r4,	0x13C8(%r5)
	
#hc	803F0CB4->80319894	__CARDTxHandler

	lis		%r4,	0x8032
	subi	%r4,	%r4,	0x676C

	stw		%r4,	0x0CB4(%r5)

#enable IRQ
	
	lis     %r4,	0x8000
	lwz     %r0,	0xC4(%r4)

	lis		%r5,	0x20
	andc	%r0,	%r0,	%r5

	stw		%r0,	0xC4(%r4)


# send cmd
	lis     %r7,	0xC000
	lis     %r0,	0x2000
	
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

	li      %r3,	0

	lmw     %r19, 0x2C(%sp)
	lwz     %r0,	0x64(%sp)
	addi    %sp, %sp, 0x60
	mtlr    %r0
	blr
