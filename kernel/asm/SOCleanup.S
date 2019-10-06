#include <asm.h>
.include "constants.inc"

SOCleanup:
	lis 0, SOCleanupAddr@h
	ori 0, 0, SOCleanupAddr@l
	mtctr 0
	bctr
