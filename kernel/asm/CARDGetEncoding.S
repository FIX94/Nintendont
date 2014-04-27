#include <asm.h>

#
#	r3	Channel
#	r4	endcoding(u16)

CARDGetEncoding:

	cmpwi	%r3,	0
	beq		DoCode
	li		%r3,	-3
	blr

DoCode:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x10(%sp)	

#0 is USA/EUR, 1 is JAP
	li		%r0,	0
	sth		%r0,	0(%r4)

end:
	li		%r3,	0

	lwz     %r0, 0x14(%sp)
	addi    %sp, %sp, 0x10
	mtlr    %r0
	blr
