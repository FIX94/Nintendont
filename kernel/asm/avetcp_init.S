#include <asm.h>
.include "constants.inc"

avetcp_init:
	lis 0, avetcp_init_Addr@h
	ori 0, 0, avetcp_init_Addr@l
	mtctr 0
	bctr
