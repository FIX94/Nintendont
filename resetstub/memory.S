.set r0,0;		.set r1,1;		.set r2,2;		.set r3,3;		.set r4,4;
.set r5,5;		.set r6,6;		.set r7,7;		.set r8,8;		.set r9,9;
.set r10,10;	.set r11,11;	.set r12,12;	.set r13,13;	.set r14,14;
.set r15,15;	.set r16,16;	.set r17,17;	.set r18,18;	.set r19,19;
.set r20,20;	.set r21,21;	.set r22,22;	.set r23,23;	.set r24,24;
.set r25,25;	.set r26,26;	.set r27,27;	.set r28,28;	.set r29,29;
.set r30,30;	.set r31,31;

#include "hw.h"

.global __temp_abe
__temp_abe:
	mfspr   r3,rHID0
	ori     r4,r3,HID0_ABE	#HID0[ABE]
	mtspr   rHID0,r4
	isync
	sync    
	mtspr   rHID0,r3
	rfi

.global DCFlashInvalidate
DCFlashInvalidate:
	nop
	mfspr   r3,rHID0
	ori     r3,r3,HID0_DCFI		#HID0[DCFI]
	mtspr   rHID0,r3
	blr

.global DCacheEnable
DCacheEnable:
	sync    
	mfspr   r3,rHID0
	ori     r3,r3,HID0_DCE		#HID0[DCE]
	mtspr   rHID0,r3
	blr

.global DCBlockInvalidate
DCBlockInvalidate:
	cmplwi  r4,0
	blelr   
	clrlwi. r5,r3,27
	beq-	0f
	addi    r4,r4,0x20
0:
	addi    r4,r4,0x1F
	rlwinm  r4,r4,27,5,31
	mtctr   r4
1:
	dcbi    r0,r3
	addi    r3,r3,0x20
	bdnz+   1b
	blr

DCBlockFlushSc:
	cmplwi  r4,0
	blelr   
	clrlwi. r5,r3,27
	beq-    0f
	addi    r4,r4,0x20
0:
	addi    r4,r4,0x1F
	rlwinm  r4,r4,27,5,31
	mtctr   r4
1:
	dcbf    r0,r3
	addi    r3,r3,0x20
	bdnz+   1b
	sc      
	blr

DCBlockFlush:
	cmplwi  r4,0
	blelr   
	clrlwi. r5,r3,27
	beq-    0f
	addi    r4,r4,0x20
0:
	addi    r4,r4,0x1F
	rlwinm  r4,r4,27,5,31
	mtctr   r4
1:
	dcbf    r0,r3
	addi    r3,r3,0x20
	bdnz+   1b
	blr

.global ICInvalidateRange
ICInvalidateRange:
	cmplwi  r4,0
	blelr   
	clrlwi. r5,r3,27
	beq-    0f
	addi    r4,r4,0x20
0:
	addi    r4,r4,0x1F
	rlwinm  r4,r4,27,5,31
	mtctr   r4
1:
	icbi    r0,r3
	addi    r3,r3,0x20
	bdnz+   1b
	sync    
	isync
	blr

ICFlashInvalidate:
	mfspr   r3,rHID0
	ori     r3,r3,HID0_ICFI		#HID0[ICFI]
	mtspr   rHID0,r3
	blr

.global ICacheEnable
ICacheEnable:
	isync
	mfspr   r3,rHID0
	ori     r3,r3,HID0_ICE		#HID0[ICE]
	mtspr   rHID0,r3
	blr

ICacheDisable:
	isync
	mfspr   r3,rHID0
	rlwinm  r3,r3,0,17,15		#HID0[ICE]
	mtspr   rHID0,r3
	blr

ISync:
	isync
	blr

.global L2_Init
L2_Init:
	mflr    r0
	stw     r0, 0x04(r1)
	stwu    r1,-0x10(r1)
	stw     r31,0x0C(r1)
	mfmsr   r3
	mr      r31,r3
	sync    
	li      r3,MSR_IR|MSR_DR	#MSR[IR|DR]
	mtmsr   r3
	sync    
	bl      L2_Disable
	bl      L2_Invalidate
	mr      r3,r31
	mtmsr   r3
	lwz     r0, 0x14(r1)
	lwz     r31,0x0C(r1)
	mtlr    r0
	blr

.global L2_Enable
L2_Enable:
	mfl2cr  r3
	oris    r0,r3,0x8000	#L2CR[L2E]
	rlwinm  r3,r0,0,11,9	#L2CR[L2I]
	mtl2cr  r3
	blr

L2_Disable:
	sync    
	mfl2cr  r3
	clrlwi  r3,r3,1			#L2CR[L2E]
	mtl2cr  r3
	sync    
	blr

L2_Invalidate:
	mflr    r0
	stw     r0, 0x04(r1)
	stwu    r1,-0x08(r1)
	bl      L2_Disable
	mfl2cr  r3
	oris    r3,r3,0x200		#L2CR[L2I]
	mtl2cr  r3
0:
	mfl2cr  r3
	clrlwi  r0,r3,31		#L2CR[L2IP]
	cmplwi  r0,0
	bne+    0b
	mfl2cr  r3
	rlwinm  r3,r3,0,11,9	#L2CR[L2I]
	mtl2cr  r3
1:
	mfl2cr  r3
	clrlwi  r0,r3,31		#L2CR[L2IP]
	cmplwi  r0,0
	bne+    1b
	lwz     r0,0x0C(r1)
	addi    r1,r1,0x08
	mtlr    r0
	blr
