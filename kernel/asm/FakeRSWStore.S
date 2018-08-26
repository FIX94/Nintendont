#include <asm.h>
.include "constants.inc"

__OSResetSWInterruptHandler:
#original instruction
	stw		r0,	0x3000(r3)

	lis		r3,	INT_BASE@h
	lwz		r0,	RSW_INT@l(r3)
	cmpwi	r0,	0
	beq		end
#keep something in RSW_INT for OSGetResetButtonState
	lis		r0,	0x2
	stw		r0,	RSW_INT@l(r3)

end:
	blr
