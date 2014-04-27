#include <asm.h>

APRCallback:
	
	mflr	%r0
	stw		%r0,	4(%sp)
	stwu	%sp,	-0x38(%sp)
	stw		%r3,	0x34(%sp)
	
	lis		%r3,	0xC000
	lwz		%r4,	0x0050(%r3)
	cmpwi	%r4,	0
	beq		end
	li		%r0,	0
	stw		%r0,	0x0050(%r3)

	lwz		%r4,	-0x64e8(%r13)
	mtctr	%r4
#	lwz		%r3,	0x0054(%r3)
	bctrl
	
end:

	lwz       %r3, 0x34(%sp)

OSRestoreInterrupts:
	cmpwi     %r3, 0
	mfmsr     %r4
	beq       loc_80304644
	ori       %r5, %r4, 0x8000
	b         loc_80304648
loc_80304644:
	rlwinm    %r5, %r4, 0,17,15
loc_80304648:
	mtmsr     %r5
	extrwi    %r3, %r4, 1,16

	lwz       %r0, 0x3C(%sp)
	addi      %sp, %sp, 0x38
	mtlr      %r0 
	blr
