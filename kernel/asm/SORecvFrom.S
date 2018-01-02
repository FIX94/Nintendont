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

SORecvFrom:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)
	stw		%r29,	0x10(%sp)
	stw		%r30,	0x14(%sp)
	stw		%r31,	0x18(%sp)
	mfmsr	%r29
	mr		%r30,	%r3

	slwi %r10, %r30, 5
	lis %r11, SO_IOCTL@h
	add %r11, %r11, %r10
	lis %r31, SO_BUSY@h
	slwi %r10, %r30, 2
	add %r31, %r31, %r10

	ori %r10, %r29, 0x8000
	mtmsr %r10
tryLock:
	lwz %r8, SO_BUSY@l(%r31)
	cmplwi %r8, 0
	bne tryLock
	rlwinm %r0, %r29, 0,17,15
	mtmsr %r0
	lwz %r8, SO_BUSY@l(%r31)
	cmplwi %r8, 0
	beq lock
	#no access, try again
	mtmsr %r10
	b tryLock
lock:
	li %r8, 1
	stw %r8, SO_BUSY@l(%r31)

	stw %r3, SO_CMD_0@l(%r11)
	stw %r4, SO_CMD_1@l(%r11)
	stw %r5, SO_CMD_2@l(%r11)
	stw %r6, SO_CMD_3@l(%r11)
	stw %r7, SO_CMD_4@l(%r11)

	li %r0, 0xC
	stw %r0, SO_IOCTL@l(%r11)
repeat:
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
	lwz %r7, SO_CMD_4@l(%r11)
	cmplwi %r7, 0
	beq loadRes
	lwz %r0, SO_CMD_2@l(%r11)
	stw %r0, 0(%r7)
	lwz %r0, SO_CMD_3@l(%r11)
	stw %r0, 4(%r7)
loadRes:
	lwz %r3, SO_RETVAL@l(%r11)
	cmpwi %r3, 0
	ble unlock
	mtctr %r3
	lwz %r4, SO_CMD_0@l(%r11)
	lwz %r5, SO_CMD_1@l(%r11)
writeOutRes:
	lbz %r0, 0(%r4)
	stb %r0, 0(%r5)
	addi %r4, %r4, 1
	addi %r5, %r5, 1
	bdnz writeOutRes
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
