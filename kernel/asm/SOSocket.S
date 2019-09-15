#include <asm.h>
.include "constants.inc"

SOSocket:
	lis 0, SOSocketAddr@h
	ori 0, 0, SOSocketAddr@l
	mtctr 0
	bctr
