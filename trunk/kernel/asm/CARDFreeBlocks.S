#include <asm.h>

#
#	r3	chan
#	r4	byteNotUsed
#	r5	filesNotUsed

CARDFreeBlocks:


	mflr	%r0
	cmpwi	%r3, 0
	beq		CARDPresent

	li      %r3, -3
	b		end

CARDPresent:

	lis		%r3, 0x7F
	addi	%r3, %r3, 0x6000
	stw		%r3, 0(%r4)

	li		%r3, 16
	stw		%r3, 0(%r5)

	li		%r3, 0

end:

	mtlr    %r0
	blr
