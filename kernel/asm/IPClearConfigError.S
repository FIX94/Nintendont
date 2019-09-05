#include <asm.h>

.set IPClearConfigErrorAddr,	0x93006054

IPClearConfigError:
	lis 0, IPClearConfigErrorAddr@h
	ori 0, 0, IPClearConfigErrorAddr@l
	mtctr 0
	bctr
