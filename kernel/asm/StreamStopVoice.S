#include <asm.h>


StreamStopVoice:

#	void AXSetVoiceState	(AXVPB *pvpb, u32 state);

	lis		%r4,	0x800B
	ori		%r4,	%r4,	0xDA8C
	mtctr   %r4
	
	lis		%r3,	0x8000
	lwz		%r3,	0x18(%r3)

	li		%r4,	0

	bctr
