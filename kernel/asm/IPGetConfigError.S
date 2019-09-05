#include <asm.h>

.set IPGetConfigErrorAddr,	0x9300604C

IPGetConfigError:
	lis 0, IPGetConfigErrorAddr@h
	ori 0, 0, IPGetConfigErrorAddr@l
	mtctr 0
	bctr
