# EXIGetID(): Get the ID of an EXI device.
# Input:
# - r3: Channel number.
# - r4: Device number.
# - r5: ID buffer.
# Output:
# - r3: Device ID, or 0 if not valid.

#include <asm.h>
.include "constants.inc"

EXIGetID:
	# Check for device 0.
	# Slot A == Ch0,Dev0
	# Slot B == Ch1,Dev1
	cmpwi	r4,	0
	beq	IsDev0
End:
	# Not device 0.
	li	r3,	0
	blr

IsDev0:
	# Check for Slot A.
	cmpwi	r3,	0
	beq	CardIsValid

	# Check for Slot B.
	cmpwi	r3,	1
	bne	End

	# Is Slot B enabled?
	lis	r4,	NinCfgConfig@h
	lwz	r4,	NinCfgConfig@l(r4)
	andis.	r4,	r4,	NIN_CFG_MC_SLOTB@h	# Check if a Slot B image was loaded.
	beq	End

CardIsValid:
	# Store 0x00000080 in the ID buffer.
	# FIXME: This means Memory Card 2043?
	li	r4,	0x80
	stw	r4,	0x0(r5)

	# Return the Dolphin OS global value for this channel.
	# Slot A info is stored at 0x800030C0.
	# Slot B info is stored at 0x800030C4.
	slwi	r3,	r3,	2
	lis	r4,	0x8000
	or	r4,	r4,	r3	# r4 = 0x80000000 + (channel * 4)
	lwz	r3,	0x30C0(r4)

	blr
