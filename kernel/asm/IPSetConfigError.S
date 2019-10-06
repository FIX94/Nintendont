#include <asm.h>
.include "constants.inc"

IPSetConfigError:
	lis 0, IPSetConfigErrorAddr@h
	ori 0, 0, IPSetConfigErrorAddr@l
	mtctr 0
	bctr
