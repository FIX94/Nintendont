#include <asm.h>

#
#	r3	Channel

CARDGetEncoding:

	cmpwi	%r3,	0
	beq		DoCode
	blr

DoCode:

	lis		%r3,	0xC000
	lwz		%r3,	0x2FA0(%r3)

	blr
