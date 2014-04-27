#include <asm.h>

#
#	r3	Channel
#	r4	workarea
#	r5	detachCallback
#	r6	attachCallback

CARDMountAsync:

	cmpwi	%r3,	0
	beq		DoCode
	li		%r3,	-3
	lis		%r7,	0xC000
	stw     %r3,	0x2F94(%r7)
	blr

DoCode:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x10(%sp)	

	li		%r3,	0
	li		%r4,	0

	cmpwi	%r6,	0
	beq		end
	
	mtctr	%r6
	bctrl

	lis		%r7,	0xC000
	li		%r3,	4
	stw     %r3,	0x2F9C(%r7)

end:
	li		%r3,	0
	li		%r4,	0
	lis		%r7,	0xC000
	stw     %r3,	0x2F94(%r7)

	lwz     %r0, 0x14(%sp)
	addi    %sp, %sp, 0x10
	mtlr    %r0
	blr
