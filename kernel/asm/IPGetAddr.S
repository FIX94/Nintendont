#include <asm.h>
.include "constants.inc"

IPGetAddr:
	lis 0, IPGetAddrAddr@h
	ori 0, 0, IPGetAddrAddr@l
	mtctr 0
	bctr
