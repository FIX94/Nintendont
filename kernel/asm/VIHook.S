# M15 preserves the M14 final-return hook, delegating to a reserved PPC module.
# Preserve every register the integer-only C renderer may touch, including flags.
.section .text
.globl VIHook
VIHook:
    stwu 1,-160(1)
    stw 0,8(1)
    stmw 3,16(1)
    mflr 0
    stw 0,132(1)
    mfcr 0
    stw 0,136(1)
    mfctr 0
    stw 0,140(1)
    mfxer 0
    stw 0,144(1)
    lis 3,0x9318
    ori 3,3,4
    mtctr 3
    bctrl
    lwz 0,144(1)
    mtxer 0
    lwz 0,140(1)
    mtctr 0
    lwz 0,136(1)
    mtcr 0
    lwz 0,132(1)
    mtlr 0
    lmw 3,16(1)
    lwz 0,8(1)
    addi 1,1,160
    blr
