#include <asm.h>


AXSetVoiceState:

	lwz		%r3,	0x1AA(%r30)
	oris	%r3,	%r3,	0x2000
	stw		%r3,	0x1AA(%r30)

	lwz		%r3,	0x1AE(%r30)
	oris	%r3,	%r3,	0x2000
	stw		%r3,	0x1AE(%r30)

	lwz		%r3,	0x1B2(%r30)
	oris	%r3,	%r3,	0x2000
	stw		%r3,	0x1B2(%r30)	

	stw		%r0,	0x1C(%r30)

	blr
