#include <asm.h>
.include "constants.inc"

IPClearConfigError:
	lis 0, IPClearConfigErrorAddr@h
	ori 0, 0, IPClearConfigErrorAddr@l
	mtctr 0
	bctr
