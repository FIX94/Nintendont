#include <asm.h>

.set SOListenAddr,	0x93006014

SOListen:
	lis 0, SOListenAddr@h
	ori 0, 0, SOListenAddr@l
	mtctr 0
	bctr
