#include <asm.h>

.set SOFcntlAddr,	0x93006030

SOFcntl:
	lis 0, SOFcntlAddr@h
	ori 0, 0, SOFcntlAddr@l
	mtctr 0
	bctr
