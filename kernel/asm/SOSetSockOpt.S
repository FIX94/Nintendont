#include <asm.h>
.include "constants.inc"

SOSetSockOpt:
	lis 0, SOSetSockOptAddr@h
	ori 0, 0, SOSetSockOptAddr@l
	mtctr 0
	bctr
