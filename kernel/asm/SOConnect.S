#include <asm.h>
.include "constants.inc"

SOConnect:
	lis 0, SOConnectAddr@h
	ori 0, 0, SOConnectAddr@l
	mtctr 0
	bctr