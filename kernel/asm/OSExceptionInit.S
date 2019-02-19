
#include <asm.h>
OSExceptionInit:
	#original instruction
	nop
	#r3 is the dest of this handler
	cmpwi r3, 0xF00
	#smaller or equal is actually ok
	ble end
	#everything above though...
	li r3, 0xF00 #grab low mem!
end:
	blr
