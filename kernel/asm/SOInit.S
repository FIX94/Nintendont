#include <asm.h>

.set SOInitAddr,	0x93006000

SOInit:
	lis 0, SOInitAddr@h
	ori 0, 0, SOInitAddr@l
	mtctr 0
	bctr
