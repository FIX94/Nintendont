#include <asm.h>
.include "constants.inc"

tcp_create:
	lis 0, tcp_create_Addr@h
	ori 0, 0, tcp_create_Addr@l
	mtctr 0
	bctr
