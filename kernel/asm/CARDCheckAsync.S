#include <asm.h>

#
#	r3	Channel
#	r4	cb( chan, res )

CARDCheck:

	mflr	%r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x10(%sp)	

	cmpwi	%r3, 0
	beq		CARDPresent

	cmpwi	%r4, 0
	beq		NoCardNoCB
	
	mtctr	%r4
	li		%r4, -3
	bctrl

NoCardNoCB:
	li      %r3, -3
	b		end

CARDPresent:

	cmpwi	%r4, 0
	beq		CardNoCB

	mtctr	%r4
	li		%r4, 0
	bctrl

CardNoCB:
	li      %r3, 0
end:
	lwz     %r0, 0x14(%sp)
	addi    %sp, %sp, 0x10
	mtlr    %r0
	blr

