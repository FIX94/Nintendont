#include <asm.h>

.set IPGetMacAddrAddr,	0x93006038

IPGetMacAddr:
	lis 0, IPGetMacAddrAddr@h
	ori 0, 0, IPGetMacAddrAddr@l
	mtctr 0
	bctr
