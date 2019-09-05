#include <asm.h>

.set IPGetAddrAddr,	0x93006040

IPGetAddr:
	lis 0, IPGetAddrAddr@h
	ori 0, 0, IPGetAddrAddr@l
	mtctr 0
	bctr
