#include <asm.h>
.include "constants.inc"

dns_close:
	lis 0, dns_close_Addr@h
	ori 0, 0, dns_close_Addr@l
	mtctr 0
	bctr
