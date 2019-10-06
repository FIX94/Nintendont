#include <asm.h>
.include "constants.inc"

tcp_listen:
	lis 0, tcp_listen_Addr@h
	ori 0, 0, tcp_listen_Addr@l
	mtctr 0
	bctr
