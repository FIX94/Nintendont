#include <asm.h>

.set IPGetNetmaskAddr,	0x9300603C

IPGetNetmask:
	lis 0, IPGetNetmaskAddr@h
	ori 0, 0, IPGetNetmaskAddr@l
	mtctr 0
	bctr
