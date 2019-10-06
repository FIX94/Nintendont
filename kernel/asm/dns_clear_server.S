#include <asm.h>
.include "constants.inc"

dns_clear_server:
	lis 0, dns_clear_server_Addr@h
	ori 0, 0, dns_clear_server_Addr@l
	mtctr 0
	bctr
