#include <asm.h>
.include "constants.inc"

SOClose:
	lis 0, SOCloseAddr@h
	ori 0, 0, SOCloseAddr@l
	mtctr 0
	bctr
