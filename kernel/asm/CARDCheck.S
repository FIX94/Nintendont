#include <asm.h>

#
#	r3	Channel

CARDCheck:

	mflr	%r0
	cmpwi	%r3, 0
	beq		CARDPresent

	li      %r3, -3
	b		end

CARDPresent:
	li		%r3, 0
end:

	lis		%r4,	0xC000
	stw     %r3,	0x2F94(%r4)

	mtlr    %r0
	blr
