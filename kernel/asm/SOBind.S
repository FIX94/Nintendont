#include <asm.h>

.set SOBindAddr,	0x9300601C

SOBind:
	lis 0, SOBindAddr@h
	ori 0, 0, SOBindAddr@l
	mtctr 0
	bctr
