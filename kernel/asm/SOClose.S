#include <asm.h>

.set SOCloseAddr,	0x93006010

SOClose:
	lis 0, SOCloseAddr@h
	ori 0, 0, SOCloseAddr@l
	mtctr 0
	bctr
