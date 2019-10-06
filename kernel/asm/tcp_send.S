#include <asm.h>
.include "constants.inc"

tcp_send:
	lis 0, tcp_send_Addr@h
	ori 0, 0, tcp_send_Addr@l
	mtctr 0
	bctr
