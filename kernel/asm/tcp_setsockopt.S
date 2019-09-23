#include <asm.h>
.include "constants.inc"

tcp_setsockopt:
	lis 0, tcp_setsockopt_Addr@h
	ori 0, 0, tcp_setsockopt_Addr@l
	mtctr 0
	bctr
