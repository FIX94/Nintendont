#include <asm.h>
.include "constants.inc"

tcp_connect:
	lis 0, tcp_connect_Addr@h
	ori 0, 0, tcp_connect_Addr@l
	mtctr 0
	bctr
