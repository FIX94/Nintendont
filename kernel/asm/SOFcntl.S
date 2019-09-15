#include <asm.h>
.include "constants.inc"

SOFcntl:
	lis 0, SOFcntlAddr@h
	ori 0, 0, SOFcntlAddr@l
	mtctr 0
	bctr
