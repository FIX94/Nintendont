#include <asm.h>

.set SOSetSockOptAddr,	0x9300602C

SOSetSockOpt:
	lis 0, SOSetSockOptAddr@h
	ori 0, 0, SOSetSockOptAddr@l
	mtctr 0
	bctr
