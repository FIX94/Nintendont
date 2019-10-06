#include <asm.h>
.include "constants.inc"

IPGetMacAddr:
	lis 0, IPGetMacAddrAddr@h
	ori 0, 0, IPGetMacAddrAddr@l
	mtctr 0
	bctr
