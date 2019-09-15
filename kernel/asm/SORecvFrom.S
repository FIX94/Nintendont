#include <asm.h>
.include "constants.inc"

SORecvFrom:
	lis 0, SORecvFromAddr@h
	ori 0, 0, SORecvFromAddr@l
	mtctr 0
	bctr
