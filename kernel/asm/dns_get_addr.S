#include <asm.h>
.include "constants.inc"

dns_get_addr:
	lis 0, dns_get_addr_Addr@h
	ori 0, 0, dns_get_addr_Addr@l
	mtctr 0
	bctr
