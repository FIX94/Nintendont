#include <asm.h>
.include "constants.inc"

tcp_bind:
	lis 0, tcp_bind_Addr@h
	ori 0, 0, tcp_bind_Addr@l
	mtctr 0
	bctr
