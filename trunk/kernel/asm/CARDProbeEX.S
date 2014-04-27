#include <asm.h>

#
#	r3	Channel
#	r4	MemSize (can be zero ptr)
#	r5	SecSize (can be zero ptr)

CARDProbeEX:


	mflr	%r0
	cmpwi	%r3, 0
	beq		CARDPresent

	li      %r3, -3
	b		end

CARDPresent:
	
	cmpwi	%r4, 0
	beq ZeroMemPtr

	li		%r3, 8
	stw		%r3, 0(%r4)

ZeroMemPtr:

	cmpwi	%r5, 0
	beq ZeroSecPtr

	li		%r3, 0x2000
	stw		%r3, 0(%r5)

ZeroSecPtr:

	li		%r3, 0

end:

	mtlr    %r0
	blr
