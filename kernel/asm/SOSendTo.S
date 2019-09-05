#include <asm.h>

.set SOSendToAddr,	0x93006028

SOSendTo:
	lis 0, SOSendToAddr@h
	ori 0, 0, SOSendToAddr@l
	mtctr 0
	bctr
