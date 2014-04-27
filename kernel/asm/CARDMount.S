#include <asm.h>

#
#	r3	Channel
#	r4	workarea
#	r5	detachCallback

CARDMount:

	mflr	%r0
	stw     %r0, 4(%sp)

	cmpwi	%r3, 0
	beq		CARDPresent

	li      %r3, -3
	b		end

CARDPresent:
	
	li		%r3, 0
end:

	lwz     %r0, 4(%sp)
	mtlr    %r0
	blr
