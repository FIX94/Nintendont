#include <asm.h>
.include "constants.inc"

DHCP_request_nb:
	lis 0, DHCP_request_nb_Addr@h
	ori 0, 0, DHCP_request_nb_Addr@l
	mtctr 0
	bctr
