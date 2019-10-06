#include <asm.h>
.include "constants.inc"

tcp_delete:
	lis 0, tcp_delete_Addr@h
	ori 0, 0, tcp_delete_Addr@l
	mtctr 0
	bctr
