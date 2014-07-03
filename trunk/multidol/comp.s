
# Routines for saving integer registers, called by the compiler.
# Called with r11 pointing to the stack header word of the caller of the
# function, just beyond the end of the integer save area.

	.globl	_savegpr_29
	.globl	_savegpr_30
	.globl	_savegpr_31

_savegpr_29:
	stw	29,-12(11)
_savegpr_30:
	stw	30,-8(11)
_savegpr_31:
	stw	31,-4(11)
	blr

# Routines for restoring integer registers, called by the compiler.
# Called with r11 pointing to the stack header word of the caller of the
# function, just beyond the end of the integer restore area.

	.globl	_restgpr_29_x
	.globl	_restgpr_30_x
	.globl	_restgpr_31_x

_restgpr_29_x:
	lwz	29,-12(11)
_restgpr_30_x:
	lwz	30,-8(11)
_restgpr_31_x:
	lwz	0,4(11)
	lwz	31,-4(11)
	mtlr	0
	mr	1,11
	blr
