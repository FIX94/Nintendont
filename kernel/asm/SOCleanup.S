#include <asm.h>

.set SOCleanupAddr,	0x93006008

SOCleanup:
	lis 0, SOCleanupAddr@h
	ori 0, 0, SOCleanupAddr@l
	mtctr 0
	bctr
