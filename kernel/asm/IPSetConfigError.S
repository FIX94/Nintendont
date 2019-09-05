#include <asm.h>

.set IPSetConfigErrorAddr,	0x93006050

IPSetConfigError:
	lis 0, IPSetConfigErrorAddr@h
	ori 0, 0, IPSetConfigErrorAddr@l
	mtctr 0
	bctr
