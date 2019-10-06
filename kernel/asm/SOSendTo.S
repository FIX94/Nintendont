#include <asm.h>
.include "constants.inc"

SOSendTo:
	lis 0, SOSendToAddr@h
	ori 0, 0, SOSendToAddr@l
	mtctr 0
	bctr
