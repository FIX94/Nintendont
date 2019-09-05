#include <asm.h>

.set SORecvFromAddr,	0x93006024

SORecvFrom:
	lis 0, SORecvFromAddr@h
	ori 0, 0, SORecvFromAddr@l
	mtctr 0
	bctr
