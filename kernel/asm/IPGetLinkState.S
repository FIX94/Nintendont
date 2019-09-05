#include <asm.h>

.set IPGetLinkStateAddr,	0x93006048

IPGetLinkState:
	lis 0, IPGetLinkStateAddr@h
	ori 0, 0, IPGetLinkStateAddr@l
	mtctr 0
	bctr
