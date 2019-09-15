#include <asm.h>
.include "constants.inc"

IPGetMtu:
	lis 0, IPGetMtuAddr@h
	ori 0, 0, IPGetMtuAddr@l
	mtctr 0
	bctr
