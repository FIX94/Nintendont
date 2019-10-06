#include <asm.h>
.include "constants.inc"

SOAccept:
	lis 0, SOAcceptAddr@h
	ori 0, 0, SOAcceptAddr@l
	mtctr 0
	bctr
