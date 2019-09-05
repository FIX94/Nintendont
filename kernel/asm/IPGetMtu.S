#include <asm.h>

.set IPGetMtuAddr,	0x93006044

IPGetMtu:
	lis 0, IPGetMtuAddr@h
	ori 0, 0, IPGetMtuAddr@l
	mtctr 0
	bctr
