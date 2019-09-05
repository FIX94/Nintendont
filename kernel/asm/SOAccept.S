#include <asm.h>

.set SOAcceptAddr,	0x93006018

SOAccept:
	lis 0, SOAcceptAddr@h
	ori 0, 0, SOAcceptAddr@l
	mtctr 0
	bctr
