#include <asm.h>

.set SOShutdownAddr,	0x93006020

SOShutdown:
	lis 0, SOShutdownAddr@h
	ori 0, 0, SOShutdownAddr@l
	mtctr 0
	bctr
