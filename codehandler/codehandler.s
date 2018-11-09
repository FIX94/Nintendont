.text

.set r0,0;   .set r1,1;   .set r2,2; .set r3,3;   .set r4,4
.set r5,5;   .set r6,6;   .set r7,7;   .set r8,8;   .set r9,9
.set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
.set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
.set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
.set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
.set r30,30; .set r31,31; .set f0,0; .set f2,2; .set f3,3

.globl _start

cheatdata:
.long	frozenvalue
.space 39*4

_start:
	stwu	r1,-168(r1)		# stores sp
	stw	r0,8(r1)		# stores r0

	mflr	r0
	stw	r0,172(r1)		# stores lr

	mfcr	r0
	stw	r0,12(r1)		# stores cr

	mfctr	r0
	stw	r0,16(r1)		# stores ctr

	mfxer	r0
	stw	r0,20(r1)		# stores xer

	stmw	r3,24(r1)		# saves r3-r31

	mfmsr	r20
	ori	r26,r20,0x2000		#enable floating point ?
	andi.	r26,r26,0xF9FF
	mtmsr	r26

	stfd	f2,152(r1)		# stores f2
	stfd	f3,160(r1)		# stores f3

    lis	r31,_start@h

    lis r3, 0xCC00
    lhz r28, 0x4010(r3)
    ori r21, r28, 0xFF
    sth r21, 0x4010(r3) # disable MP3 memory protection

	bl	_codehandler

_setvalues:
	li	r21,0
	li	r22,0x19
	li	r23,0xD0
	lis	r24,0xCD00

	ori	r18, r31, frozenvalue@l	# read buffer just store in lowmem
	lwz	r0,172(r1)		# loads lr
	stw	r0,4(r18)		# stores lr
    stw r21, 0x643C(r24)		# exi speed up

frozen:
    bl	exireceivebyte
    beq	finish			# r3 returns 1 or 0, one for byte received ok

checkcommand:

    cmpwi	   r29, 0x04		# checks lf 8/1/32 bits write command
    bge	   _nextcommand
    cmpwi      r29, 0x01
    blt	   finish
    b          writedword		#write value to address
_nextcommand:
    beq        readmem
    cmpwi        r29, 0x06
    beq        freezegame
    cmpwi        r29, 0x07
    beq        unfreezegame
    cmpwi        r29, 0x08
    beq        resumegame
    cmpwi        r29, 0x09
    beq        breakpoints #ibp
    cmpwi        r29, 0x10
    beq        breakpoints #dbp
    cmpwi        r29, 0x2F
    beq        upbpdata
    cmpwi        r29, 0x30
    beq        getbpdata
    cmpwi        r29, 0x38
    beq       cancelbreakpoints
    cmpwi        r29, 0x40
    beq        sendcheats
    cmpwi        r29, 0x41
    beq        uploadcode
    cmpwi	     r29, 0x44
    beq	   breakpoints #step
    cmpwi        r29, 0x50
    beq        pausestatus
    cmpwi        r29, 0x60
    beq        executecodes
    cmpwi        r29, 0x89
    beq        breakpoints #aligned dbp
    cmpwi        r29, 0x99
    beq	   versionnumber
    b          finish

#***************************************************************************
#                        subroutine: getpausestatus:
#                one  running, two  paused, three  on a breakpoint
#***************************************************************************

pausestatus:
	lwz	r3,0(r18)
	bl	exisendbyte
	b	finish


#***************************************************************************
#                        subroutine: executecodes:
#                executes the code handler
#***************************************************************************

executecodes:
	bl	_codehandler
	b	finish


#***************************************************************************
#                        subroutine: freezegame:
#                Write a 1 to a memory location to freeze
#***************************************************************************

freezegame:
	li	r4, 1
	stw	r4, 0(r18)
	b	finish

#***************************************************************************
#                        subroutine: upload bp data:
#                receive the dp databuffer from the PC
#***************************************************************************

upbpdata:
	bl	exisendbyteAA

	li	r16, 40*4		# 42 "registers rN" * 4             
	ori	r12, r31, regbuffer@l
	b	upload

#***************************************************************************
#                        subroutine: getbp data:
#                send the dp databuffer to the PC
#***************************************************************************

getbpdata:
	li	r3, 72*4		# register buffer            
	ori	r12, r31, regbuffer@l
	bl	exisendbuffer		# send it, less than a packet
	b	finish


#***************************************************************************
#                        subroutine: breakpoints:
#               handles data/instruction address breakpoints
#***************************************************************************

breakpoints:

	cmpwi	cr6,r29,0x10		# 0x10 = instruction, 0x09/0x89 = data
	cmpwi	cr5,r29,0x44		# 0x44 = step

	ori	r4,r31,bphandler@l	#used for writebranch (= source address)

	lis	r3,0x8000
	ori	r3,r3,0x300		# bprw  0x80000300
	bl	writebranch

	addi	r3,r3,0xA00		# trace 0x80000D00
	bl	writebranch

	addi	r3,r3,0x600		# bpx   0x80001300
	bl	writebranch

	ori	r12, r31, bpbuffer@l	# read buffer just store in lowmem

	stw	r21,0(r12)		# clears inst bp
	stw	r21,4(r12)		# clears data bp
	stw	r21,8(r12)		# clears alignement

	ori	r4, r31, regbuffer@l
	lwz	r9,0x18(r4)

	lwz	r3,0(r18)
	cmpwi	r3,2			# checks lf the gecko is on a breakpoint
	bne	+12
	beq	cr5,+12
	b	bp
	li	r3,0
	stw	r3,12(r12)		# lf not, clears the previous "broken on" address

	bne	bp

	bne	cr5,bp
	ori	r9,r9,0x400
	stw	r9,0x18(r4)
	b	unfreezegame

bp:
	rlwinm	r9,r9,0,22,20
	stw	r9,0x18(r4)
	beq	cr5,finish
	beq	cr6,+8
	addi	r12,r12,4		# lf databp r12=r12+4

	li	r3, 4			# 4 bytes
	bl	exireceivebuffer

	ble	cr6,noalignement	# lf not aligned data bp (0x89)

	addi	r12,r12,4

	li	r3, 4			# 4 bytes
    bl	exireceivebuffer

noalignement:
	ori	r4, r31, bpbuffer@l	# read buffer just store in lowmem

    lwz	r3, 0(r4)
    lwz	r4, 4(r4)
    mtspr	1010, r3		# set IABR
	mtspr	1013, r4		# set DABR
	b	finish

#***************************************************************************
#                        subroutine: bphandler
#    Data/Instruction address breakpoint handler, save context and return
#***************************************************************************

bphandler:
	mtsprg	2,r1			# sprg2 = r1
	mfsrr0	r1			# r1=srr0		
	mtsprg	3,r3			# sprg3 = r3
	mfsrr1	r3
			# r3=srr1
	rlwinm	r3,r3,0,22,20		# clear trace
	stw	r3,regbuffer@l+0x18(r0)	# store srr1 with trace cleared in regbuffer
	rlwinm	r3,r3,0,24,15
	ori	r3,r3,0x2000
#	rlwinm	r3,r3,0,17,15		# clear hw interrupt
	mtsrr1	r3			# restore srr1 with hw interrupt & trace cleared
	
	lis	r3,break@h
	ori	r3,r3,break@l
	mtsrr0	r3
	rfi

break:
	lis	r3, regbuffer@h
	ori	r3, r3, regbuffer@l
	stw	r1,0x14(r3)		#stores srr0

	mr	r1,r3
	mfsprg	r3,3
	stmw	r2,0x24(r1)		#saves r2-r31
	mr	r4,r1
b break_get_r1
	.long 0 #this is 0x1300, so breakpoint possible
break_get_r1:
	mfsprg	r1,2

	stw	r0,0x1C(r4)
	stw	r1,0x20(r4)

	mflr	r3
	stw	r3, 0x9C(r4)		# Store LR

	mfcr	r3
	stw	r3, 0x0(r4)		# Store CR

	mfxer	r3
	stw	r3, 0x4(r4)		# Store XER

	mfctr	r3
    stw	r3, 0x8(r4)		# Store CTR

    mfdsisr	r3
    stw	r3, 0xC(r4)		# Store DSISR
       
    mfdar	r3
    stw	r3, 0x10(r4)		# Store DAR

	li	r9,0
	mtspr	1010,r9			# Clear IABR
	mtspr	1013,r9			# Clear DABR

    lis r5,floatstore@h
    ori r5,r5,floatstore@l # Set r5 to instruction address
    lis r31,0xD004
    ori r31,r31,0x00A0 # Set r31 to 'stfs f0,0xA0(r4)' (==0xD00400A0)
    floatloop:
    stw r31,0(r5)
    dcbst r0,r5
    sync
    icbi r0,r5
    isync
    floatstore:
    stfs	f0,0xA0(r4)
    addi r31,r31,4 # Advance instruction offset
    addis r31,r31,0x20 # Advance instruction register
    rlwinm. r16,r31,0,5,5 # Check for register > 31
    beq floatloop

skip_floating_point:
	lis	r31,_start@h
	ori	r5,r31,bpbuffer@l

	lwz	r16,0(r5)
	lwz	r17,4(r5)
	lwz	r19,12(r5)

	cmpwi	r19,0
	beq	_redobp

	cmpwi	r19,2
	bne	+24
	lwz	r9,0x14(r4)
	addi	r9,r19,3
	stw	r9,0(r5)
	stw	r9,12(r5)
	b	_executebp

	cmpw	r16,r19
	beq	_step

	cmpw	r17,r19
	beq	_step

	add	r9,r16,r17
	stw	r9,12(r5)

_alignementcheck:
	lwz	r16,8(r5)
	cmpwi	r16,0
	beq	_executebp		# no alignement = normal break

	lwz	r3,0x10(r4)
	cmpw	r16,r3			# we check lf address = aligned address
	bne	_step			# lf no, we need to set a bp on the next instruction

	li	r16,0
	stw	r16,8(r5)		# lf we are on the good address we clear the aligned bp check
	b	_executebp		# and we break

_step:
	li	r17,0
	stw	r17,12(r5)
	lwz	r9,0x18(r4)
	ori	r9,r9,0x400
	stw	r9,0x18(r4)
	b	_skipbp			#and we don't break right now

_redobp:
	mtspr	1010,r16		# we set back the instbp with the original value
	mtspr	1013,r17			# we set back the databp with the original value
	li	r9,1
	stw	r9,12(r5)
	b	_skipbp			# and we don't break

_executebp:
	li	r5, 2
	ori	r4, r31, frozenvalue@l	# Freeze once returned to let user know there is a breakpoint hit
	stw	r5, 0(r4)

	li	r3, 0x11
	bl	exisendbyte		# tell the PC a bp has happened (send 0x11)

	bl	_start			# bl mainloop, so you can set up a new breakpoint.

_skipbp:
	mfmsr	r1
	rlwinm	r1,r1,0,31,29
	rlwinm	r1,r1,0,17,15
	mtmsr	r1			# we disable the interrupt so nothing interfers with the restore

	ori	r1, r31, regbuffer@l

	lwz	r3,0x0(r1)
	mtcr	r3			# restores CR
	lwz	r3,0x14(r1)
	mtsrr0	r3			# restores SRR0
	lwz	r3,0x18(r1)
	mtsrr1	r3			# restores SRR1
	lwz	r3,0x9C(r1)
	mtlr	r3			# restores LR

	lmw	r2,0x24(r1)		# restores r2-r31

	lwz	r0,0x1C(r1)		# restores r0
	lwz	r1,0x20(r1)		# restores r1


	rfi				# back to the game

#***************************************************************************
#                        subroutine: unfreezegame:
#                Write a 0 to a memory location to unfreeze
#***************************************************************************

unfreezegame:
	stw	r21, 0(r18)
	b	resumegame

#***************************************************************************
#                        subroutine: write dword:
#                Write a long word value to memory sent from PC
#***************************************************************************

writedword:
	cmpwi	cr5,r29,2
	li	r3, 8			# 8 bytes (location 4 / Value 4)

	ori	r12, r31, dwordbuffer@l	# buffer
	bl	exireceivebuffer

    lwz	r5, 0(r12)
    lwz	r3, 4(r12)		# read the value	
    stb	r3, 0(r5)		# write to location
	blt	cr5,skipp
    sth	r3, 0(r5)		# write to location
	beq	cr5,skipp
    stw	r3, 0(r5)		# write to location
	skipp:
    dcbf    r0, r5                # data cache block flush
	sync
    icbi    r0, r5
	isync
    b	finish

#***************************************************************************
#                        subroutine: SendCheats :
#                        Fill memory with value
#***************************************************************************

sendcheats:
	bl	exisendbyteAA

	li	r3, 4			# 4 bytes
	ori	r12, r31, dwordbuffer@l	# buffer
	bl	exireceivebuffer

	lwz	r16, 0(r12)

	lis r12, codelist@h
	ori	r12, r12, codelist@l

	b	upload

#***************************************************************************
#                        subroutine: Upload :
#                        Fill memory with value
#***************************************************************************

uploadcode:
	bl	exisendbyteAA

	li	r3, 8			# 8 bytes
	ori	r12, r31, dwordbuffer@l	# buffer

	bl	exireceivebuffer
          
	lwz	r16, 4(r12)
	lwz	r12, 0(r12)

upload:	
	ori	r27, r31, rem@l		# Remainder of bytes upload code
	li	r17,0xF80
	bl	_packetdivide
	beq	douploadremainder

nextrecpacket:
	mr	r3,r17
	bl	exireceivebuffer	# r12 start = start of buffer

uploadwait:
	bl	exisendbyteAA
	beq	uploadwait
	add	r12,r12,r14

	subic.	r11, r11, 1		# decrease loop counter     
	bgt	nextrecpacket

douploadremainder:			# send the remainder bytes
	lwz	r3, 0(r27)		# remainder
	cmpwi	r3,0
	beq	finishupload
	bl	exireceivebuffer
finishupload:
    dcbf    r0, r12                # data cache block flush
	sync
    icbi    r0, r12                # instruction cache block flush
	isync
	b	finish

#***************************************************************************
#                        subroutine: exireceivebyte:
#         Return 1(r3) lf byte receive, 0(r3) lf no byte
#         Command byte is stored at 0x800027ff
#***************************************************************************

exireceivebyte:
	mflr	r30
    lis	r3, 0xA000		# EXI read command

	bl checkexisend

	andis.	r3, r16, 0x800
	rlwinm	r29,r16,16,24,31
	mtlr	r30
	blr

#***************************************************************************
#                        subroutine: checkexisend:
#  
#***************************************************************************

checkexisend:
	stw	r23, 0x6814(r24)		# 32mhz Port B
	stw	r3, 0x6824(r24)
	stw	r22, 0x6820(r24)          # 0xCC006820 (Channel 1 Control Register)

exicheckreceivewait:                
 	lwz	r5, 0x6820(r24)
	andi.	r5, r5, 1
	bne	exicheckreceivewait	# while((exi_chan1cr)&1);

	lwz	r16, 0x6824(r24)
	stw	r5, 0x6814(r24)

	blr

#***************************************************************************
#                        subroutine: exireceivebuffer:
#  r3 byte counter, r12 points to buffer, r3 gets copied as gets destroyed  
#***************************************************************************

exireceivebuffer:
	mflr	r10			# save link register
	mtctr	r3			# counter
	li	r14,0

bufferloop:
	bl	exicheckreceive
	bl	exicheckreceive
	bl	exireceivebyte   
	beq	bufferloop		# r3 returns 1 or 0, one for byte received ok

	stbx	r29, r14,r12        	# store byte into buffer

	addi	r14, r14, 1        	# increase buffer by 1
	bdnz	bufferloop

	mtlr	r10			# retore link register
	blr				# return to command check

#***************************************************************************
#                        exisendbuffer:
#  r3 byte counter, r12 points to buffer, r3 gets copied as gets destroyed
#***************************************************************************

exisendbuffer:
	mflr	r10			# save link register
	mtctr	r3			# r3->counter
	li	r14,0

sendloop:
	lbzx	r3, r12,r14
	bl	exisendbyte   
	beq	sendloop

	addi	r14, r14, 1		# increase buffer
	bdnz	sendloop
	mtlr	r10			# restore link register
	blr

#***************************************************************************
#                        exisendbyte:12345678
#  r3 byte to send, returns 1 lf sent, 0 lf fail (!!! called by breakpoint)
#***************************************************************************
exisendbyteAA:
	li	r3,0xAA

exisendbyte:				# r3, send value
	mflr	r30
	slwi	r3, r3, 20		# (sendbyte<<20);
	oris	r3, r3, 0xB000		# 0xB0000000 | (OR)
	li	r22,0x19
	li	r23,0xD0
	lis	r24,0xCD00

	bl checkexisend

	extrwi.  r3, r16, 1,5		# returns either 0 or 1, one for byte received ok
	mtlr	r30
	blr

#***************************************************************************
#                        subroutine: exicheckrecieve:
#         Return 1(r3) lf byte receive, 0(r3) lf no byte
#***************************************************************************

exicheckreceive:
	mflr	r30
exicheckreceive2:
	lis	r3, 0xD000		# EXI check status command

	bl checkexisend

	rlwinm.	r3,r16,6,31,31		# returns either 0 or 1 for r3

	beq     exicheckreceive2
	mtlr	r30
	blr

#***************************************************************************
#                Readmem: (reads a memory range back to PC)
#                r3 byte to send, returns 1 lf sent, 0 lf fail
#***************************************************************************

readmem:
	bl	exisendbyteAA

    li	r3, 8			# 8 bytes
	ori	r12, r31, dwordbuffer@l	# buffer

	bl	exireceivebuffer

	lwz	r5, 4(r12)
	lwz	r12, 0(r12)

	ori	r27, r31, rem@l		# place holder for remainder bytes
        
	ori	r17, r21, 0xF800		# 64K packet
	sub	r16, r5, r12		# memrange = (*endlocation - *startlocation)

	bl	_packetdivide

nextsendpacket:
	bgt	transfer		#compares r11 and 0
	lwz	r17, 0(r27)		# remainder
	cmpwi	r17,0
	beq	finish
transfer:
	mr	r3,r17		# number of bytes
	bl	exisendbuffer

bytewait1:
	bl	exicheckreceive
	bl	exicheckreceive
	bl	exireceivebyte
	beq	bytewait1		# r3 returns 1 or 0, one for byte received ok
	cmpwi	r29, 0xCC		# cancel code
	beq	finish
	cmpwi	r29, 0xBB		# retry code
	beq	transfer
	cmpwi	r29, 0xAA
	bne	bytewait1
	add	r12,r12,r14
	subic.	r11, r11, 1		# decrease loop counter
	blt	finish
	b	nextsendpacket

#***************************************************************************
#                Cancel Breakpoints
#                Clears instruction and data and step breakpoints
#***************************************************************************

cancelbreakpoints:
	mtspr	1013, r21		# clear the DABR
	mtspr	1010, r21		# clear the IABR
	ori	r4, r31, regbuffer@l
	lwz	r9,0x18(r4)
	rlwinm	r9,r9,0,22,20
	stw	r9,0x18(r4)
	b finish

#***************************************************************************
#                        subroutine: version number
#                Sends back the current gecko version number.
#***************************************************************************

versionnumber:
	li	r3, 0x80		#0x80 = Wii, 0x81 = NGC
	bl	exisendbyte
	#b	finish

#***************************************************************************
#                Finish
#                Check lf the gecko has been paused. lf no return to game
#***************************************************************************

finish:
	lwz	r4, 0(r18)
	cmpwi	r4, 0			# check to see lf we have frozen the game
	bne	frozen			# loop around lf we have (changed to return for the bp)

resumegame:
    lis r3, 0xCC00
    sth r28,0x4010(r3)  # restore memory protection value

	lfd	f2,152(r1)		# loads f2
	lfd	f3,160(r1)		# loads f3

	mtmsr	r20         # restore msr

	lwz	r0,172(r1)
	mtlr	r0			# restores lr

	lwz	r0,12(r1)
	mtcr	r0			# restores cr

	lwz	r0,16(r1)
	mtctr	r0			# restores ctr

	lwz	r0,20(r1)
	mtxer	r0			# restores xer

	lmw	r3,24(r1)		# restores r3-r31

	lwz	r0,8(r1)		# loads r0

	addi	r1,r1,168

	isync

    blr				# return back to game

#***************************************************************************
#                        Write branch
#    r3 - source (our mastercode location)
#    r4 - destination (lowmem area 0x80001800 address which will branch to
#***************************************************************************

writebranch:
	subf	r17, r3, r4		# subtract r3 from r4 and place in r17
	lis	r5, 0x4800		# 0x48000000
	rlwimi	r5,r17,0,6,29
	stw	r5, 0(r3)		# result in r3
	
    dcbf    r0, r3                # data cache block flush
	sync
    icbi    r0, r3
	isync

	blr				# return


#***************************************************************************
#                        Packet Divide
#  Used by the down/up routines to calculate the number of packet to send
#***************************************************************************

_packetdivide:
	divw.	r11, r16, r17		# fullpackets = memrange / 0xF80 (r11 is full packets)
	mullw	r10, r11, r17
	subf	r10, r10, r16		# r10 holds remainder byte counter
	stw	r10, 0(r27)		# store remainder
	blr

#================================================================================

_codehandler:
	mflr	r29

	# Expect the kernel to write the codelist pointer
	lis	r13, codelist_addr@h
	ori	r13, r13, codelist_addr@l
	lwz	r15, 0(r13)

	ori	r7, r31, cheatdata@l	# set pointer for storing data (before the codelist)

	lis	r6,0x8000		# default base address = 0x80000000 (code handler)

	mr	r16,r6			# default pointer =0x80000000 (code handler)

	li	r8,0			# code execution status set to true (code handler)

	lis	r3,0x00D0
    ori	r3,r3,0xC0DE

	lwz	r4,0(r15)
	cmpw	r3,r4
	bne-	_exitcodehandler
	lwz	r4,4(r15)
	cmpw	r3,r4
    bne-	_exitcodehandler	# lf no code list skip code handler
    addi	r15,r15,8
	b	_readcodes

_exitcodehandler:
	mtlr	r29
    blr

_readcodes:
	lwz	r3,0(r15)		#load code address
	lwz	r4,4(r15)		#load code value

	addi	r15,r15,8		#r15 points to next code

	andi.	r9,r8,1
	cmpwi	cr7,r9,0		#check code execution status in cr7. eq = true, ne = false

	li	r9,0			#Clears r9

	rlwinm	r10,r3,3,29,31		#r10 = extract code type, 3 bits
	rlwinm 	r5,r3,7,29,31		#r5  = extract sub code type 3 bits

	andis.	r11,r3,0x1000		#test pointer
	rlwinm	r3,r3,0,7,31		#r3  = extract address in r3 (code type 0/1/2) #0x01FFFFFF

	bne	+12			#jump lf the pointer is used

	rlwinm	r12,r6,0,0,6		#lf pointer is not used, address = base address
	b	+8

	mr	r12,r16			#lf pointer is used, address = pointer

	cmpwi	cr4,r5,0		#compares sub code type with 0 in cr4

	cmpwi	r10,1
	blt+	_write			#code type 0 : write
	beq+	_conditional		#code type 1 : conditional

	cmpwi	r10,3
	blt+	_ba_pointer		#Code type 2 : base address operation

	beq-	_repeat_goto		#Code type 3 : Repeat & goto

	cmpwi	r10,5
	b		_readcodes_1808
#this is 0x1800 here so gameid is expected
	gameid: .long 0,0
_readcodes_1808:
	blt-	_operation_rN		#Code type 4 : rN Operation
	beq+	_compare16_NM_counter	#Code type 5 : compare [rN] with [rM]

	cmpwi	r10,7
	blt+	_hook_execute		#Code type 6 : hook, execute code
	
	b	_terminator_onoff_	#code type 7 : End of code list

#CT0=============================================================================
#write  8bits (0): 00XXXXXX YYYY00ZZ
#write 16bits (1): 02XXXXXX YYYYZZZZ
#write 32bits (2): 04XXXXXX ZZZZZZZZ
#string code  (3): 06XXXXXX YYYYYYYY, d1d1d1d1 d2d2d2d2, d3d3d3d3 ....
#Serial Code  (4): 08XXXXXX YYYYYYYY TNNNZZZZ VVVVVVVV

_write:
	add	r12,r12,r3		#address = (ba/po)+(XXXXXX)
	cmpwi	r5,3
	beq-	_write_string		#r5  == 3, goto string code
	bgt-	_write_serial		#r5  >= 4, goto serial code

	bne-	cr7,_readcodes		#lf code execution set to false skip code

	cmpwi	cr4,r5,1		#compares sub code type and 1 in cr4

	bgt-	cr4,_write32		#lf sub code type == 2, goto write32

	#lf sub code type = 0 or 1 (8/16bits)
	rlwinm	r10,r4,16,16,31		#r10 = extract number of times to write (16bits value)

_write816:
	beq	cr4,+16			#lf r5 = 1 then 16 bits write
	stbx	r4,r9,r12		#write byte
	addi	r9,r9,1
	b	+12
	sthx	r4,r9,r12		#write halfword
	addi	r9,r9,2
	subic.	r10,r10,1		#number of times to write -1
	bge-	_write816
	b	_readcodes

_write32:
	rlwinm	r12,r12,0,0,29		#32bits align adress
    stw	r4,0(r12)		#write word to address
    b	_readcodes

_write_string:				#endianess ?
	mr	r9,r4
	bne-	cr7,_skip_and_align	#lf code execution is false, skip string code data

	_stb:
	subic.	r9,r9,1			#r9 -= 1 (and compares r9 with 0)
	blt-	_skip_and_align		#lf r9 < 0 then exit
	lbzx	r5,r9,r15
	stbx	r5,r9,r12		#loop until all the data has been written
	b	_stb

_write_serial:
	addi	r15,r15,8		#r15 points to the code after the serial code
	bne-	cr7,_readcodes		#lf code execution is false, skip serial code

	lwz	r5,-8(r15)		#load TNNNZZZZ
	lwz	r11,-4(r15)		#r11 = load VVVVVVVV

	rlwinm	r17,r5,0,16,31		#r17 = ZZZZ
	rlwinm	r10,r5,16,20,31		#r10 = NNN (# of times to write -1)
	rlwinm	r5,r5,4,28,31		#r5  = T (0:8bits/1:16bits/2:32bits)

_loop_serial:
	cmpwi	cr5,r5,1
	beq-	cr5,+16			#lf 16bits
	bgt+	cr5,+20			#lf 32bits
	
	stbx	r4,r9,r12		#write serial byte (CT04,T=0)
	b	+16

	sthx	r4,r9,r12		#write serial halfword (CT04,T=1)
	b	+8

	stwx	r4,r9,r12		#write serial word (CT04,T>=2)

	add	r4,r4,r11		#value +=VVVVVVVV
	add	r9,r9,r17		#address +=ZZZZ
	subic.	r10,r10,1
	bge+	_loop_serial		#loop until all the data has been written

	b	_readcodes

#CT1=============================================================================
#32bits conditional (0,1,2,3): 20XXXXXX YYYYYYYY
#16bits conditional (4,5,6,7): 28XXXXXX ZZZZYYYY

#PS : 31 bit of address = endlf.

_conditional:
	rlwinm.	r9,r3,0,31,31		#r10 = (bit31 & 1) (endlf enabled?)

	beq	+16			#jump lf endlf is not enabled

	rlwinm	r8,r8,31,1,31		#Endlf (r8>>1)
	andi.	r9,r8,1			#r9=code execution status
	cmpwi	cr7,r9,0		#check code execution status in cr7
	cmpwi	cr5,r5,4		#compares sub code type and 4 in cr5
	cmpwi	cr3,r10,5		#compares code type and 5 in cr3

	rlwimi	r8,r8,1,0,30		#r8<<1 and current execution status = old execution status
	bne-	cr7,_true_end		#lf code execution is set to false -> exit

	bgt	cr3,_addresscheck2	#lf code type==6 -> address check
	add	r12,r12,r3		#address = (ba/po)+(XXXXXX)

	blt	cr3,+12			#jump lf code type <5 (==1)
	blt	cr5,_condition_sub	#compare [rN][rM]
	b	_conditional16_2	#counter compare
	bge	cr5,_conditional16	#lf sub code type>=4 -> 16 bits conditional

_conditional32:
	rlwinm	r12,r12,0,0,29		#32bits align
	lwz	r11,0(r12)
	b	_condition_sub

_conditional16:
	rlwinm	r12,r12,0,0,30		#16bits align
	lhz	r11,0(r12)
_conditional16_2:
	nor	r9,r4,r4
	rlwinm	r9,r9,16,16,31		#r9  = extract mask
	and	r11,r11,r9		#r11 &= r9
	rlwinm	r4,r4,0,16,31		#r4  = extract data to check against

_condition_sub:
	cmpl	cr6,r11,r4		#Unsigned compare. r11=data at address, r4=YYYYYYYY
	andi.	r9,r5,3
	beq	_skip_NE		#lf sub code (type & 3) == 0
	cmpwi	r9,2
	beq	_skip_LE		#lf sub code (type & 3) == 2
	bgt	_skip_GE		#lf sub code (type & 3) == 3

_skip_EQ:#1
	bne-	cr6,_true_end		#CT21, CT25, CT29 or CT2D (lf !=)
	b	_skip

_skip_NE:#0
	beq-	cr6,_true_end		#CT20, CT24, CT28 or CT2C (lf==)
	b	_skip

_skip_LE:#2
	bgt-	cr6,_true_end		#CT22, CT26, CT2A or CT2E (lf r4>[])
	b	_skip

_skip_GE:#3
	blt-	cr6,_true_end		#CT23, CT27, CT2B or CT2F (lf r4<r4)

_skip:
	ori	r8,r8,1			#r8|=1 (execution status set to false)
_true_end:
	bne+	cr3,_readcodes		#lf code type <> 5
	blt	cr5,_readcodes
	lwz	r11,-8(r15)		#load counter
	bne	cr7,_clearcounter	#lf previous code execution false clear counter
	andi.	r12,r3,0x8		#else lf clear counter bit not set increase counter
	beq	_increase_counter
	andi.	r12,r8,0x1		#else lf.. code result true clear counter
	beq	_clearcounter

_increase_counter:
	addi	r12,r11,0x10		#else increase the counter
	rlwimi	r11,r12,0,12,27		#update counter
	b	_savecounter

_clearcounter:
	rlwinm	r11,r11,0,28,11		#clear the counter
_savecounter:
	stw	r11,-8(r15)		#save counter
	b _readcodes


#CT2============================================================================

#load base adress    (0): 40TYZ00N XXXXXXXX = (load/add:T) ba from [(ba/po:Y)+XXXXXXXX(+rN:Z)]

#set base address    (1): 42TYZ00N XXXXXXXX = (set/add:T) ba to (ba/po:Y)+XXXXXXXX(+rN:Z)

#store base address  (2): 440Y0000 XXXXXXXX = store base address to [(ba/po)+XXXXXXXX]
#set base address to (3): 4600XXXX 00000000 = set base address to code address+XXXXXXXX
#load pointer        (4): 48TYZ00N XXXXXXXX = (load/add:T) po from [(ba/po:Y)+XXXXXXXX(+rN:Z)]

#set pointer         (5): 4ATYZ00N XXXXXXXX = (set/add:T) po to (ba/po:Y)+XXXXXXXX(+rN:Y)

#store pointer       (6): 4C0Y0000 XXXXXXXX = store pointer to [(ba/po)+XXXXXXXX]
#set pointer to      (7): 4E00XXXX 00000000 = set pointer to code address+XXXXXXXX

_ba_pointer:
	bne-	cr7,_readcodes

	rlwinm	r9,r3,2,26,29		#r9  = extract N, makes N*4

	rlwinm	r14,r3,16,31,31		#r3 = add ba/po flag bit (Y)
	cmpwi	cr3,r14,0

	cmpwi	cr4,r5,4		#cr4 = compare sub code type with 4 (ba/po)
	andi.	r14,r5,3		#r14 = sub code type and 3
	
	cmpwi	cr5,r14,2		#compares sub code type and 2

	blt-	cr5,_p01
	beq-	cr5,_p2			#sub code type 2

_p3:
	extsh	r4,r3
	add	r4,r4,r15		#r4=XXXXXXXX+r15 (code location in memory)
	b	_pend

_p01:
	rlwinm.	r5,r3,20,31,31		#r3 = rN use bit (Z)
	beq	+12			#flag is not set(=0), address = XXXXXXXX

	lwzx	r9,r7,r9		#r9 = load register N
	add	r4,r4,r9		#flag is set (=1), address = XXXXXXXX+rN

	beq	cr3,+8			#(Y) flag is not set(=0), address = XXXXXXXX (+rN)
 
  	add	r4,r12,r4		#address = XXXXXXXX (+rN) + (ba/po)

	cmpwi	cr5,r14,1			
	beq	cr5,+8			#address = (ba/po)+XXXXXXXX(+rN)
	lwz	r4,0(r4)		#address = [(ba/po)+XXXXXXXX(+rN)]

	rlwinm.	r3,r3,12,31,31		#r5 = add/replace flag (T)
	beq	_pend			#flag is not set (=0), (ba/po)= XXXXXXXX (+rN) + (ba/po)
	bge	cr4,+12
	add	r4,r4,r6		#ba += XXXXXXXX (+rN) + (ba/po)
	b	_pend
	add	r4,r4,r16		#po += XXXXXXXX (+rN) + (ba/po)
	b	_pend		

_p2:
	rlwinm.	r5,r3,20,31,31		#r3 = rN use bit (Z)
	beq	+12			#flag is not set(=0), address = XXXXXXXX

	lwzx	r9,r7,r9		#r9 = load register N
	add	r4,r4,r9		#flag is set (=1), address = XXXXXXXX+rN

	bge	cr4,+12
	stwx	r6,r12,r4		#[(ba/po)+XXXXXXXX] = base address
	b	_readcodes
	stwx	r16,r12,r4		#[(ba/po)+XXXXXXXX] = pointer
	b	_readcodes

_pend:
	bge	cr4,+12
	mr	r6,r4			#store result to base address
	b	_readcodes
	mr	r16,r4			#store result to pointer
	b	_readcodes	


#CT3============================================================================
#set repeat     (0): 6000ZZZZ 0000000P = set repeat
#execute repeat (1): 62000000 0000000P = execute repeat
#return		(2): 64S00000 0000000P = return (lf true/false/always)
#goto		(3): 66S0XXXX 00000000 = goto (lf true/false/always)
#gosub		(4): 68S0XXXX 0000000P = gosub (lf true/false/always)

_repeat_goto:
	rlwinm	r9,r4,3,25,28		#r9  = extract P, makes P*8
	addi	r9,r9,0x40		#offset that points to block P's
	cmpwi	r5,2			#compares sub code type with 2
	blt-	_repeat

	rlwinm.	r11,r3,10,0,1		#extract (S&3)
	beq	+20			#S=0, skip lf true, don't skip lf false
	bgt	+8
	b	_b_bl_blr_nocheck	#S=2/3, always skip (code exec status turned to true)
	beq-	cr7,_readcodes		#S=1, skip lf false, don't skip lf true
	b	_b_bl_blr_nocheck

_b_bl_blr:
	bne-	cr7,_readcodes		#lf code execution set to false skip code

_b_bl_blr_nocheck:
	cmpwi	r5,3

	bgt-	_bl			#sub code type >=4, bl
	beq+	_b			#sub code type ==3, b

_blr:
	lwzx	r15,r7,r9		#loads the next code address
	b	_readcodes

_bl:
	stwx	r15,r7,r9		#stores the next code address in block P's address
_b:
	extsh	r4,r3			#XXXX becomes signed
	rlwinm	r4,r4,3,9,28

	add	r15,r15,r4		#next code address +/-=line XXXX
	b	_readcodes

_repeat:
	bne-	cr7,_readcodes		#lf code execution set to false skip code

	add	r5,r7,r9		#r5 points to P address
	bne-	cr4,_execute_repeat	#branch lf sub code type == 1

_set_repeat:
	rlwinm	r4,r3,0,16,31		#r4  = extract NNNNN
	stw	r15,0(r5)		#store current code address to [bP's address]
	stw	r4,4(r5)		#store NNNN to [bP's address+4]

	b	_readcodes

_execute_repeat:
	lwz	r9,4(r5)		#load NNNN from [M+4]
	cmpwi	r9,0
	beq-	_readcodes
	subi	r9,r9,1
	stw	r9,4(r5)		#saves (NNNN-1) to [bP's address+4]
	lwz	r15,0(r5)		#load next code address from [bP's address]
	b	_readcodes

#CT4============================================================================
#set/add to rN(0) : 80SY000N XXXXXXXX = rN = (ba/po) + XXXXXXXX
#load rN      (1) : 82UY000N XXXXXXXX = rN = [XXXXXXXX] (offset support) (U:8/16/32)
#store rN     (2) : 84UYZZZN XXXXXXXX = store rN in [XXXXXXXX] (offset support) (8/16/32)

#operation 1  (3) : 86TY000N XXXXXXXX = operation rN?XXXXXXXX ([rN]?XXXXXXXX)
#operation 2  (4) : 88TY000N 0000000M = operation rN?rM ([rN]?rM, rN?[rM], [rN]?[rM])

#copy1        (5) : 8AYYYYNM XXXXXXXX = copy YYYY bytes from [rN] to ([rM]+)XXXXXXXX
#copy2        (6) : 8CYYYYNM XXXXXXXX = copy YYYY bytes from ([rN]+)XXXXXX to [rM]


#for copy1/copy2, lf register == 0xF, base address is used.

#of course, sub codes types 0/1, 2/3 and 4/5 can be put together lf we need more subtypes.


_operation_rN:
	bne-	cr7,_readcodes

	rlwinm	r11,r3,2,26,29		#r11  = extract N, makes N*4
	add	r26,r7,r11		#1st value address = rN's address
	lwz	r9,0(r26)		#r9 = rN

	rlwinm	r14,r3,12,30,31		#extracts S, U, T (3bits)

	beq-	cr4,_op0		#lf sub code type = 0

	cmpwi	cr4,r5,5
	bge-	cr4,_op56			#lf sub code type = 5/6

	cmpwi	cr4,r5,3
	bge-	cr4,_op34			#lf sub code type = 3/4

	cmpwi	cr4,r5,1

_op12:	#load/store
	rlwinm.	r5,r3,16,31,31		#+(ba/po) flag : Y
	beq	+8			#address = XXXXXXXX
	add	r4,r12,r4

	cmpwi	cr6,r14,1
	bne-	cr4,_store

_load:
	bgt+	cr6,+24
	beq-	cr6,+12

	lbz	r4,0(r4)		#load byte at address
	b	_store_reg

	lhz	r4,0(r4)		#load halfword at address
	b	_store_reg

	lwz	r4,0(r4)		#load word at address
	b	_store_reg

_store:
	rlwinm	r19,r3,28,20,31		#r9=r3 ror 12 (N84UYZZZ)

_storeloop:
	bgt+	cr6,+32
	beq-	cr6,+16

	stb	r9,0(r4)		#store byte at address
	addi	r4,r4,1
	b	_storeloopend

	sth	r9,0(r4)		#store byte at address
	addi	r4,r4,2
	b	_storeloopend

	stw	r9,0(r4)		#store byte at address
	addi	r4,r4,4
_storeloopend:
	subic.	r19,r19,1
	bge 	_storeloop
	b	_readcodes

_op0:	
	rlwinm.	r5,r3,16,31,31		#+(ba/po) flag : Y
	beq	+8			#value = XXXXXXXX
	add	r4,r4,r12		#value = XXXXXXXX+(ba/po)	

	andi.	r5,r14,1		#add flag : S
	beq	_store_reg		#add flag not set (=0), rN=value
	add	r4,r4,r9		#add flag set (=1), rN=rN+value
	b	_store_reg

_op34:	#operation 1 & 2
	rlwinm	r10,r3,16,30,31		#extracts Y

	rlwinm	r14,r4,2,26,29		#r14  = extract M (in r4), makes M*=4

	add	r19,r7,r14		#2nd value address = rM's address
	bne	cr4,+8
	subi	r19,r15,4		#lf CT3, 2nd value address = XXXXXXXX's address

	lwz	r4,0(r26)		#1st value = rN

	lwz	r9,0(r19)		#2nd value = rM/XXXXXXXX

	andi.	r11,r10,1		#lf [] for 1st value
	beq	+8
	mr	r26,r4			

	andi.	r11,r10,2		#lf [] for 2nd value
	beq	+16
	mr	r19,r9
	bne+	cr4,+8	
	add	r19,r12,r19		#lf CT3, 2nd value address = XXXXXXXX+(ba/op)

	rlwinm.	r5,r3,12,28,31		#operation # flag : T

	cmpwi	r5,9
	bge	_op_float

_operation_bl:
	bl	_operation_bl_return

_op450:
	add	r4,r9,r4		#N + M
	b	_store_reg

_op451:
	mullw	r4,r9,r4		#N * M
	b	_store_reg

_op452:
	or	r4,r9,r4		#N | M
	b	_store_reg

_op453:
	and	r4,r9,r4		#N & M
	b	_store_reg

_op454:
	xor	r4,r9,r4		#N ^ M
	b	_store_reg

_op455:
	slw	r4,r9,r4		#N << M
	b	_store_reg

_op456:
	srw	r4,r9,r4		#N >> M
	b	_store_reg

_op457:
	rlwnm	r4,r9,r4,0,31		#N rol M
	b	_store_reg

_op458:
	sraw	r4,r9,r4		#N asr M

_store_reg:
	stw	r4,0(r26)		#Store result in rN/[rN]
	b	_readcodes

_op_float:
	cmpwi	r5,0xA
	bgt	_readcodes

	lfs	f2,0(r26)		#f2 = load 1st value
	lfs	f3,0(r19)		#f3 = load 2nd value
	beq-	_op45A

_op459:
	fadds	f2,f3,f2		#N = N + M (float)
	b	_store_float

_op45A:
	fmuls	f2,f3,f2		#N = N * M (float)

_store_float:
	stfs	f2,0(r26)		#Store result in rN/[rN]
	b	_readcodes

_operation_bl_return:
	mflr	r10
	rlwinm	r5,r5,3,25,28		#r5 = T*8
	add	r10,r10,r5		#jumps to _op5: + r5

	lwz	r4,0(r26)		#load [rN]
	lwz	r9,0(r19)		#2nd value address = rM/XXXXXXXX

	mtlr	r10
	blr

#copy1        (5) : 8AYYYYNM XXXXXXXX = copy YYYY bytes from [rN] to ([rM]+)XXXXXXXX
#copy2        (6) : 8CYYYYNM XXXXXXXX = copy YYYY bytes from ([rN]+)XXXXXX to [rM]

_op56:				

	bne-	cr7,_readcodes		#lf code execution set to false skip code

	rlwinm	r9,r3,24,0,31		#r9=r3 ror 8 (NM8AYYYY, NM8CYYYY)
	mr	r14,r12			#r14=(ba/po)
	bl	_load_NM

	beq-	cr4,+12   
	add	r17,r17,r4		#lf sub code type==0 then source+=XXXXXXXX
	b	+8
	add	r9,r9,r4		#lf sub code type==1 then destination+=XXXXXXXX

	rlwinm.	r4,r3,24,16,31		#Extracts YYYY, compares it with 0
	li	r5,0

	_copy_loop:
	beq 	_readcodes		#Loop until all bytes have been copied.
	lbzx 	r10,r5,r17
	stbx 	r10,r5,r9
	addi	r5,r5,1
	cmpw	r5,r4
	b 	_copy_loop


#===============================================================================
#This is a routine called by _memory_copy and _compare_NM_16

_load_NM:				
	cmpwi	cr5,r10,4		#compare code type and 4(rn Operations) in cr5

	rlwinm 	r17,r9,6,26,29		#Extracts N*4
	cmpwi 	r17,0x3C
	lwzx	r17,r7,r17		#Loads rN value in r17
	bne 	+8
	mr	r17,r14			#lf N==0xF then source address=(ba/po)(+XXXXXXXX, CT5)

	beq	cr5,+8
	lhz	r17,0(r17)		#...and lf CT5 then N = 16 bits at [XXXXXX+base address]

	rlwinm 	r9,r9,10,26,29		#Extracts M*4
	cmpwi 	r9,0x3C
	lwzx	r9,r7,r9		#Loads rM value in r9
	bne 	+8
	mr	r9,r14			#lf M==0xF then dest address=(ba/po)(+XXXXXXXX, CT5)

	beq	cr5,+8
	lhz	r9,0(r9)		#...and lf CT5 then M = 16 bits at [XXXXXX+base address]

	blr

#CT5============================================================================
#16bits conditional (0,1,2,3): A0XXXXXX NM00YYYY (unknown values)
#16bits conditional (4,5,6,7): A8XXXXXX ZZZZYYYY (counter)

#sub codes types 0,1,2,3 compare [rN] with [rM] (both 16bits values)
#lf register == 0xF, the value at [base address+XXXXXXXX] is used.

_compare16_NM_counter:
	cmpwi 	r5,4
	bge	_compare16_counter

_compare16_NM:
	mr	r9,r4			#r9=NM00YYYY

	add	r14,r3,r12		#r14 = XXXXXXXX+(ba/po)

	rlwinm	r14,r14,0,0,30		#16bits align (base address+XXXXXXXX)

	bl	_load_NM		#r17 = N's value, r9 = M's value

	nor	r4,r4,r4		#r4=!r4
	rlwinm	r4,r4,0,16,31		#Extracts !YYYY

	and	r11,r9,r4		#r3 = (M AND !YYYY)
	and	r4,r17,r4		#r4 = (N AND !YYYY)

	b _conditional

_compare16_counter:
	rlwinm	r11,r3,28,16,31		#extract counter value from r3 in r11
	b _conditional

#===============================================================================
#execute     (0) : C0000000 NNNNNNNN = execute
#hook1       (2) : C4XXXXXX NNNNNNNN = insert instructions at XXXXXX
#hook2       (3) : C6XXXXXX YYYYYYYY = branch from XXXXXX to YYYYYY
#on/off      (6) : CC000000 00000000 = on/off switch
#range check (7) : CE000000 XXXXYYYY = is ba/po in XXXX0000-YYYY0000

_hook_execute:
	mr	r26,r4			#r18 = 0YYYYYYY
	rlwinm	r4,r4,3,0,28		#r4  = NNNNNNNN*8 = number of lines (and not number of bytes)
	bne-	cr4,_hook_addresscheck	#lf sub code type != 0
	bne-	cr7,_skip_and_align

_execute:
	mtlr	r15
	blrl

_skip_and_align:
	add	r15,r4,r15
	addi	r15,r15,7
	rlwinm	r15,r15,0,0,28		#align 64-bit
	b	_readcodes

_hook_addresscheck:
	
	cmpwi	cr4,r5,3
	bgt-	cr4,_addresscheck1	#lf sub code type ==6 or 7
	lis	r5,0x4800
	add	r12,r3,r12
	rlwinm	r12,r12,0,0,29		#align address

	bne-	cr4,_hook1		#lf sub code type ==2

_hook2:
	bne-	cr7,_readcodes

	rlwinm	r4,r26,0,0,29		#address &=0x01FFFFFC

	sub	r4,r4,r12		#r4 = to-from
	rlwimi	r5,r4,0,6,29		#r5  = (r4 AND 0x03FFFFFC) OR 0x48000000
	rlwimi	r5,r3,0,31,31		#restore lr bit
	stw	r5,0(r12)		#store opcode
	b	_readcodes

_hook1:
	bne-	cr7,_skip_and_align

	sub	r9,r15,r12		#r9 = to-from
	rlwimi	r5,r9,0,6,29		#r5  = (r9 AND 0x03FFFFFC) OR 0x48000000
	stw	r5,0(r12)		#stores b at the hook place (over original instruction)
	addi	r12,r12,4
	add	r11,r15,r4
	subi	r11,r11,4		#r11 = address of the last work of the hook1 code
	sub	r9,r12,r11
	rlwimi	r5,r9,0,6,29		#r5  = (r9 AND 0x03FFFFFC) OR 0x48000000
	stw	r5,0(r11)		#stores b at the last word of the hook1 code
	b	_skip_and_align

_addresscheck1:
	cmpwi	cr4,r5,6
	beq	cr4,_onoff
	b	_conditional
_addresscheck2:
	rlwinm	r12,r12,16,16,31
	rlwinm	r4,r26,16,16,31
	rlwinm	r26,r26,0,16,31
	cmpw	r12,r4
	blt	_skip
	cmpw	r12,r26
	bge	_skip
	b	_readcodes

_onoff:
	rlwinm	r5,r26,31,31,31		#extracts old exec status (x b a)
	xori	r5,r5,1
	andi.	r3,r8,1			#extracts current exec status
	cmpw	r5,r3
	beq	_onoff_end
	rlwimi	r26,r8,1,30,30
	xori	r26,r26,2

	rlwinm.	r5,r26,31,31,31		#extracts b
	beq	+8

	xori	r26,r26,1

	stw	r26,-4(r15)		#updates the code value in the code list

_onoff_end:
	rlwimi	r8,r26,0,31,31		#current execution status = a
	
	b _readcodes

#===============================================================================
#Full terminator  (0) = E0000000 XXXXXXXX = full terminator
#Endlfs/Else      (1) = E2T000VV XXXXXXXX = endlfs (+else)
#End code handler     = F0000000 00000000

_terminator_onoff_:
	cmpwi	r11,0			#lf code type = 0xF
    beq _notTerminator
    cmpwi r5,1
    beq _asmTypeba
    cmpwi r5,2
    beq _asmTypepo
    cmpwi r5,3
    beq _patchType
    b _exitcodehandler
_asmTypeba:
    rlwinm r12,r6,0,0,6 # use base address
_asmTypepo:
    rlwinm r23,r4,8,24,31 # extract number of half words to XOR
    rlwinm r24,r4,24,16,31 # extract XOR checksum
    rlwinm r4,r4,0,24,31 # set code value to number of ASM lines only
    bne cr7,_goBackToHandler #skip code if code execution is set to false
    rlwinm. r25,r23,0,24,24 # check for negative number of half words
    mr r26,r12 # copy ba/po address
    add r26,r3,r26 # add code offset to ba/po code address
    rlwinm r26,r26,0,0,29 # clear last two bits to align address to 32-bit
    beq _positiveOffset # if number of half words is negative, extra setup needs to be done
    extsb r23,r23
    neg r23,r23
    mulli r25,r23,2
    addi r25,r25,4
    subf r26,r25,r26
_positiveOffset:
    cmpwi r23,0
    beq _endXORLoop
    li r25,0
    mtctr r23
_XORLoop:
    lhz r27,4(r26)
    xor r25,r27,r25
    addi r26,r26,2
    bdnz _XORLoop
_endXORLoop:
    cmpw r24,r25
    bne _goBackToHandler
    b _hook_execute
_patchType:
    rlwimi	r8,r8,1,0,30		#r8<<1 and current execution status = old execution status
    bne	cr7,_exitpatch		#lf code execution is set to false -> exit
    rlwinm. r23,r3,22,0,1
    bgt _patchfail
    blt _copytopo
_runpatch:
    rlwinm r30,r3,0,24,31
    mulli r30,r30,2
    rlwinm r23,r4,0,0,15
    xoris r24,r23,0x8000
    cmpwi r24,0
    bne- _notincodehandler
    ori r23,r23,0x3000
_notincodehandler:
    rlwinm r24,r4,16,0,15
    mulli r25,r30,4
    subf r24,r25,r24
_patchloop:
    li r25,0
_patchloopnext:
    mulli r26,r25,4
    lwzx r27,r15,r26
    lwzx r26,r23,r26
    addi r25,r25,1
    cmplw r23,r24
    bgt _failpatchloop
    cmpw r25,r30
    bgt _foundaddress
    cmpw r26,r27
    beq _patchloopnext
    addi r23,r23,4
    b _patchloop
_foundaddress:
    lwz r3,-8(r15)
    ori r3,r3,0x300
    stw r3,-8(r15)
    stw r23,-4(r15)
    mr r16,r23
    b _exitpatch
_failpatchloop:
    lwz r3,-8(r15)
    ori r3,r3,0x100
    stw r3,-8(r15)
_patchfail:
    ori	r8,r8,1			#r8|=1 (execution status set to false)
    b _exitpatch
_copytopo:
    mr r16,r4
_exitpatch:
    rlwinm r4,r3,0,24,31 # set code to number of lines only
_goBackToHandler:
    mulli r4,r4,8
    add r15,r4,r15 # skip the lines of the code
    b _readcodes

_notTerminator:

_terminator:
	bne	cr4,+12			#check lf sub code type == 0
	li	r8,0			#clear whole code execution status lf T=0
	b	+20

	rlwinm.	r9,r3,0,27,31		#extract VV
#	bne 	+8			#lf VV!=0
#	bne-	cr7,+16

	rlwinm	r5,r3,12,31,31		#extract "else" bit

	srw	r8,r8,r9		#r8>>VV, meaning endlf VV lfs

    rlwinm. r23,r8,31,31,31
    bne +8 # execution is false if code execution >>, so don't invert code status
	xor	r8,r8,r5		#lf 'else' is set then invert current code status

_load_baseaddress:
	rlwinm.	r5,r4,0,0,15
	beq	+8
	mr	r6,r5			#base address = r4
	rlwinm.	r5,r4,16,0,15
	beq	+8
	mr	r16,r5			#pointer = r4
	b	_readcodes

#===============================================================================
       
frozenvalue:	#frozen value, then LR
.long        0,0
dwordbuffer:
.long        0,0
rem:
.long        0
bpbuffer:
.long 0		#int address to bp on
.long 0		#data address to bp on
.long 0		#alignement check
.long 0		#counter for alignement

regbuffer:
.space 72*4

.align 3

# Expect the kernel write a pointer to the codelist that we should use
.global codelist_addr
codelist_addr:
.long 0

codelist:
.space 2*4
.end
