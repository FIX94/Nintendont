#include <asm.h>
.include "constants.inc"

tcp_accept:
	lis 0, tcp_accept_Addr@h
	ori 0, 0, tcp_accept_Addr@l
	mtctr 0
	bctr
