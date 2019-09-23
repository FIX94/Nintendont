#include <asm.h>
.include "constants.inc"

tcp_abort:
	lis 0, tcp_abort_Addr@h
	ori 0, 0, tcp_abort_Addr@l
	mtctr 0
	bctr
