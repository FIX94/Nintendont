# EXIProbe(): Check if a device is present on the given EXI channel.
# Input:
# - r3: Channel number.
# Output:
# - r3: 1 if a device is present; 0 if no device is present.

#include <asm.h>
.include "constants.inc"

EXIProbe:
	lis	r4,	NinCfgConfig@h
	lwz	r4,	NinCfgConfig@l(r4)
	andi.	r4,	r4,	0x8		# Check if memory card emulation is enabled.
	beq	False				# Memory card emulation is not enabled.

	cmpwi	r3,	0			# Check if EXIProbe() was called for Slot A.
	beq	True				# If so, the card is definitely present.

	cmpwi	r3,	1			# Check if EXIProbe() was called for Slot B.
	bne	False				# If not, the channel parameter is invalid.

	lis	r4,	NinCfgConfig@h
	lwz	r4,	NinCfgConfig@l(r4)
	andis.	r4,	r4,	NIN_CFG_MC_SLOTB@h	# Check if a Slot B image was loaded.
	bne	True

False:
	# No EXI device is present on this channel.
	li	r3,	0
	blr

True:
	# An EXI device is present on this channel.
	# Set the Dolphin OS global variable.
	# Slot A info is stored at 0x800030C0.
	# Slot B info is stored at 0x800030C4.
	slwi	r3,	r3,	2
	lis	r4,	0x8000
	or	r4,	r4,	r3	# r4 = 0x80000000 + (channel * 4)

	# Store the magic number 0xE2D383C1 in the OS Globals section
	# to indicate that a memory card is present.
	lis	r3,	0xE2D3
	ori	r3,	r3,	0x83C1
	stw	r3,	0x30C0(r4)

	# EXI device is present.
	li	r3,	1
	blr
