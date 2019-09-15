#include <asm.h>
.include "constants.inc"

SOInit:
	lis 0, SOInitAddr@h
	ori 0, 0, SOInitAddr@l
	mtctr 0
	bctr
