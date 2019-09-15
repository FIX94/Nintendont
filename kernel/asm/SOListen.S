#include <asm.h>
.include "constants.inc"

SOListen:
	lis 0, SOListenAddr@h
	ori 0, 0, SOListenAddr@l
	mtctr 0
	bctr
