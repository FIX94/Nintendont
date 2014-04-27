#include <asm.h>


StreamStartVoice:
	stwu    %sp,	-0x10(%sp)
	mflr    %r0
	stw     %r0,	8(%sp)

#	void AXSetVoiceState	(AXVPB *pvpb, u32 state);

	lis		%r4,	0x800B
	ori		%r4,	%r4,	0xDA8C
	mtctr   %r4

	lis		%r3,	0x8000
	lwz		%r4,	0x14(%r3)
	cmpwi	%r4,	1
	beq		NoStart
	
	lwz		%r3,	0x18(%r3)
	li		%r4,	1
	bctrl

NoStart:
	li		%r4,	1

	lis		%r3,	0x8000
	stw		%r4,	0x14(%r3)

end:

	lwz     %r0, 8(%sp)
	mtlr    %r0
	addi    %sp, %sp, 0x10

	blr
