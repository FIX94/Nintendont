#include <asm.h>
.include "constants.inc"

DHCP_release:
	lis 0, DHCP_release_Addr@h
	ori 0, 0, DHCP_release_Addr@l
	mtctr 0
	bctr
