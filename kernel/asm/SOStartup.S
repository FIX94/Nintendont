#include <asm.h>

.set SO_IOCTL,	0xD3026900
.set SO_RETVAL,	0xD3026904
.set SO_CMD_0,	0xD3026908
.set SO_CMD_1,	0xD302690C
.set SO_CMD_2,	0xD3026910
.set SO_CMD_3,	0xD3026914
.set SO_CMD_4,	0xD3026918
.set SO_CMD_5,	0xD302691C
.set SO_STARTED,0x930269A8
.set OSSleepThread,0x930269B0
.set SO_QUEUE,	0x930269C0
.set SO_BUSY,	0x93026A00

SOStartup:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)
	stw		%r29,	0x10(%sp)
	stw		%r30,	0x14(%sp)
	stw		%r31,	0x18(%sp)
	mfmsr	%r29
	li		%r30,	4

	slwi %r10, %r30, 5
	lis %r11, SO_IOCTL@h
	add %r11, %r11, %r10
	lis %r31, SO_BUSY@h
	slwi %r10, %r30, 2
	add %r31, %r31, %r10

tryLock:
	lwz %r8, SO_BUSY@l(%r31)
	cmplwi %r8, 0
	bne tryLock
	rlwinm %r10, %r29, 0,17,15
	mtmsr %r10
	lwz %r8, SO_BUSY@l(%r31)
	cmplwi %r8, 0
	beq lock
	#no access, try again
	mtmsr %r29
	b tryLock
lock:
	li %r8, 1
	stw %r8, SO_BUSY@l(%r31)

	li %r0, 0x1F
	stw %r0, SO_IOCTL@l(%r11)

	slwi	%r3,	%r30,	3
	lis		%r6,	SO_QUEUE@h
	ori		%r4,	%r6,	SO_QUEUE@l
	add		%r3,	%r3,	%r4
	lis		%r4,	OSSleepThread@h
	lwz		%r4,	OSSleepThread@l(%r4)
	mtlr	%r4
	blrl

	slwi %r10, %r30, 5
	lis %r11, SO_IOCTL@h
	add %r11, %r11, %r10
	lwz %r3, SO_RETVAL@l(%r11)
	cmpwi %r3, 0
	bne unlock
	li %r0, 1
	stw %r0, 0(%r13) #actual start register
	lis %r10, SO_STARTED@h
	stw %r0, SO_STARTED@l(%r10)
unlock:
	li %r8, 0
	stw %r8, SO_BUSY@l(%r31)

end:
	mtmsr	%r29
	lwz		%r0,	0xC(%sp)
	lwz		%r29,	0x10(%sp)
	lwz		%r30,	0x14(%sp)
	lwz		%r31,	0x18(%sp)
	addi	%sp,	%sp,	0x20
	mtlr	%r0
	blr
