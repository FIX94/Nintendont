#include <asm.h>

#
#	r3	Channel

CARDProbe:


	mflr	%r0
	cmpwi	%r3, 0
	beq		CARDPresent

	li      %r3, 0
	b		end

CARDPresent:
	li		%r3, 1
end:

	mtlr    %r0
	blr
