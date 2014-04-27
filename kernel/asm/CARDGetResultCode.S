#include <asm.h>

#
#	r3	Channel

CARDGetResultCode:

	cmpwi	%r3, 0
	beq		CARDPresent

	li      %r3, -3
	b		end

CARDPresent:
	lis		%r3,	0xC000
	lwz     %r3,	0x2F94(%r3)
end:

	blr
