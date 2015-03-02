#include <asm.h>

.set	INT_BASE,	0xD3026500
.set	RSW_INT,	INT_BASE+0x00

OSGetResetButtonState:
	lis		%r5,	INT_BASE@h
	lwz		%r6,	RSW_INT@l(%r5)
	cmpwi	%r6,	0
	beq		end
	mr		%r0,	%r6

end:
	rlwinm.	%r0,	%r0,	0,		15,		15
	blr
