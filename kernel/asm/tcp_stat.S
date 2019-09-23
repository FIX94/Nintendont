#include <asm.h>
.include "constants.inc"

tcp_stat:
	lis 0, tcp_stat_Addr@h
	ori 0, 0, tcp_stat_Addr@l
	mtctr 0
	bctr
