#include <asm.h>
.include "constants.inc"

DHCP_get_gateway:
	lis 0, DHCP_get_gateway_Addr@h
	ori 0, 0, DHCP_get_gateway_Addr@l
	mtctr 0
	bctr
