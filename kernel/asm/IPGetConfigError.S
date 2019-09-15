#include <asm.h>
.include "constants.inc"

IPGetConfigError:
	lis 0, IPGetConfigErrorAddr@h
	ori 0, 0, IPGetConfigErrorAddr@l
	mtctr 0
	bctr
