#include <asm.h>

.set FAKE_RSW_INT,	0xCD806838

__OSResetSWInterruptHandler:

	stw		%r0,	0x3000(%r3)

	lis		%r3,	FAKE_RSW_INT@h
	lwz		%r0,	FAKE_RSW_INT@l(%r3)
	cmpwi	%r0,	0
	beq		end
#keep something in FAKE_RSW_INT for OSGetResetButtonState
	lis		%r0,	0x2
	stw		%r0,	FAKE_RSW_INT@l(%r3)

end:
	blr
