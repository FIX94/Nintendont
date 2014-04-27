#include <asm.h>

#
#	r3	Channel
#	r4	*xfer bytes used to repair the FS
#	r5	cb

CARDCheckEx:

	mflr	%r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x10(%sp)	

	cmpwi	%r3, 0
	beq		CARDPresent

	cmpwi	%r4, 0
	beq		NoCardZeroPtr

	li		%r6, 0
	stw		%r6, 0(%r4)

NoCardZeroPtr:
	cmpwi	%r5, 0
	beq		NoCardNoCB
	
	mtctr	%r5
	li		%r4, -3
	bctrl

NoCardNoCB:
	li      %r3, -3
	b		end

CARDPresent:

	cmpwi	%r4, 0
	beq		CardZeroPtr

	li		%r6, 0
	stw		%r6, 0(%r4)

CardZeroPtr:
	cmpwi	%r5, 0
	beq		CardNoCB

	mtctr	%r5
	li		%r4, 0
	bctrl

CardNoCB:
	li      %r3, 0
end:
	lis		%r4,	0xC000
	stw     %r3,	0x2F94(%r4)

	lwz     %r0, 0x14(%sp)
	addi    %sp, %sp, 0x10
	mtlr    %r0
	blr
