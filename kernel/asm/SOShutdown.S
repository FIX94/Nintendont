#include <asm.h>
.include "constants.inc"

SOShutdown:
	lis 0, SOShutdownAddr@h
	ori 0, 0, SOShutdownAddr@l
	mtctr 0
	bctr
