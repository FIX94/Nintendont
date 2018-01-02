#include <asm.h>

.set SO_IP_ADDR,	0x930269A0

#	r3	srcbuf
#	r4	dstbuf

IPGetAddr:
	cmplwi %r3,0
	beq HostId
	lwz %r3,0x44(%r3)
	b storeIp
HostId:
	lis %r3,SO_IP_ADDR@h
	ori %r3, %r3, SO_IP_ADDR@l
	li %r0, 0
	dcbi %r0, %r3
	sync
	lwz %r3, 0(%r3)
storeIp:
	stw %r3,0(%r4)
	blr
