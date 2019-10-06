#include <asm.h>
.include "constants.inc"

DHCP_get_dns:
	lis 0, DHCP_get_dns_Addr@h
	ori 0, 0, DHCP_get_dns_Addr@l
	mtctr 0
	bctr
