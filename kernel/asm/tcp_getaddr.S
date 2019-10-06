#include <asm.h>
.include "constants.inc"

tcp_getaddr:
	lis 0, tcp_getaddr_Addr@h
	ori 0, 0, tcp_getaddr_Addr@l
	mtctr 0
	bctr
