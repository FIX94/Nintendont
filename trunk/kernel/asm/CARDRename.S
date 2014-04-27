#include <asm.h>

#
#	r3	chan
#	r4	old
#	r5	new
#	r6	cb

CARDRename:

	mflr    %r8

	cmpwi	%r3,	0
	bne		NoCard
			
#send cmd to DM

	mr		%r12,	%r6

	li		%r0,	0
	lis		%r7,	0xC000
	addi	%r7,	%r7,	0x2F60
#IPC area
	dcbi	%r0,	%r7
	addi	%r7,	%r7,	0x20
	dcbi	%r0,	%r7
	

#cache workaround for the filename
	li		%r3,	8
	mtctr	%r3
	lis     %r3,	0xC000
invalidloopA:
	lwz		%r0,	0(%r4)
	stw		%r0,	0x17C0(%r3)
	addi	%r4,	%r4,	4
	addi	%r3,	%r3,	4
	bdnz	invalidloopA

#cache workaround for the filename
	li		%r3,	8
	mtctr	%r3
	lis     %r3,	0xC000
invalidloopB:
	lwz		%r0,	0(%r5)
	stw		%r0,	0x17E0(%r3)
	addi	%r5,	%r5,	4
	addi	%r3,	%r3,	4
	bdnz	invalidloopB
	
	lis     %r7,	0xC000

	lis     %r0,	0xCB00
	stw     %r0,	0x2F60(%r7)

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
	
	cmpwi	%r12,	0
	beq		skip_cb
	mtlr	%r12
	li      %r3,	0
	lwz     %r4,	0x2F94(%r7)
	blrl

skip_cb:
	lwz     %r3,	0x2F94(%r7)
	b		end
	
NoCard:
	li		%r3,	-3
end:
	mr		%r4,	%r3

	mtlr    %r8
	blr
