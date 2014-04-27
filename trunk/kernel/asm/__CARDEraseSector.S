#include <asm.h>

# issue read command
#
#	r3	CARDBlockEntry
#	r4	
#	r5	cb

__CARDEraseSector:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x60(%sp)
	stmw    %r19, 0x2C(%sp)
	
	cmpwi	%r5,	0
	beq		skip_cb

	li		%r3,	0
	li		%r4,	0
	
	mtctr	%r5
	bctrl

skip_cb:

	li      %r3,	0

	lmw     %r19, 0x2C(%sp)
	lwz     %r0,	0x64(%sp)
	addi    %sp, %sp, 0x60
	mtlr    %r0
	blr
