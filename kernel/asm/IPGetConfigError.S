#include <asm.h>

#	r3	srcbuf

IPGetConfigError:
	cmplwi %r3,0
	beq noError
	lwz %r3,8(%r3)
	b end
noError:
	li %r3, 0
end:
	blr
