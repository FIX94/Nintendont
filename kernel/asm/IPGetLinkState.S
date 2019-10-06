#include <asm.h>
.include "constants.inc"

IPGetLinkState:
	lis 0, IPGetLinkStateAddr@h
	ori 0, 0, IPGetLinkStateAddr@l
	mtctr 0
	bctr
