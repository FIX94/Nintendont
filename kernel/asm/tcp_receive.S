#include <asm.h>
.include "constants.inc"

tcp_receive:
	lis 0, tcp_receive_Addr@h
	ori 0, 0, tcp_receive_Addr@l
	mtctr 0
	bctr
