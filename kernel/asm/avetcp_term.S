#include <asm.h>
.include "constants.inc"

avetcp_term:
	lis 0, avetcp_term_Addr@h
	ori 0, 0, avetcp_term_Addr@l
	mtctr 0
	bctr
