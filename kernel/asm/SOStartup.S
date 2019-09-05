#include <asm.h>

.set SOStartupAddr,	0x93006004

SOStartup:
	lis 0, SOStartupAddr@h
	ori 0, 0, SOStartupAddr@l
	mtctr 0
	bctr
