#include <asm.h>
.include "constants.inc"

SOStartup:
	lis 0, SOStartupAddr@h
	ori 0, 0, SOStartupAddr@l
	mtctr 0
	bctr
