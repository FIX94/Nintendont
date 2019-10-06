#include <asm.h>
.include "constants.inc"

SOBind:
	lis 0, SOBindAddr@h
	ori 0, 0, SOBindAddr@l
	mtctr 0
	bctr
