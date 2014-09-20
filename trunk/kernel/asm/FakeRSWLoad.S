#include <asm.h>

.set FAKE_RSW_INT,	0xCD806838

OSGetResetButtonState:
	lis		%r5,	FAKE_RSW_INT@h
	lwz		%r6,	FAKE_RSW_INT@l(%r5)
	cmpwi	%r6,	0
	beq		end
	mr		%r0,	%r6

end:
	rlwinm.	%r0,	%r0,	0,		15,		15
	blr
