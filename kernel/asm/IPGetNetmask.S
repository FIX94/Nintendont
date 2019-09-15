#include <asm.h>
.include "constants.inc"

IPGetNetmask:
	lis 0, IPGetNetmaskAddr@h
	ori 0, 0, IPGetNetmaskAddr@l
	mtctr 0
	bctr
