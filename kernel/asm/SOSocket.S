#include <asm.h>

.set SOSocketAddr,	0x9300600C

SOSocket:
	lis 0, SOSocketAddr@h
	ori 0, 0, SOSocketAddr@l
	mtctr 0
	bctr
