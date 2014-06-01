#include <asm.h>


PADReadSteerVSSimple:

	lis		%r3,	0xCD00
	
	cmpwi	%r4,	1
	bne		SteerXSkip
	lwz		%r3,	0x6404(%r3)
	andi.	%r3,	%r3,	0xFF00
	b		end
SteerXSkip:

	cmpwi	%r4,	3
	bne		SteerXSkip_P2
	lwz		%r3,	0x6410(%r3)
	andi.	%r3,	%r3,	0xFF00
	b		end
SteerXSkip_P2:

	cmpwi	%r4,	0
	bne		SteerYSkip
	lwz		%r3,	0x6404(%r3)
	not		%r3,	%r3
	andi.	%r3,	%r3,	0xFF
	slwi	%r3,	%r3,	8
	b		end
SteerYSkip:

	cmpwi	%r4,	2
	bne		SteerYSkip_P2
	lwz		%r3,	0x6410(%r3)
	not		%r3,	%r3
	andi.	%r3,	%r3,	0xFF
	slwi	%r3,	%r3,	8
	b		end
SteerYSkip_P2:
	li		%r3,	0
end:
	blr
