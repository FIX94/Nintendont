/*
Constants for Nintendont ARM assembler code.
Copyright (c) 2014-2017 FIX94 and Nintendont contributors.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __NINTENDONT_KERNEL_ASM_CONSTANTS_H__
#define __NINTENDONT_KERNEL_ASM_CONSTANTS_H__

/** Configuration **/

.set	NinCfg,			0xD3004000
.set	NinCfgConfig,		NinCfg + 0x8

.set	NIN_CFG_MC_SLOTB,	0x80000000

/** EXI registers **/

.set	EXI_CMD_0,		0xD3026800
.set	EXI_CMD_1,		0xD3026804

/** Interrupts **/

.set	IRQ_STATUS,		0xCC003000

/* all in different cache blocks */
.set	INT_BASE,		0xD3026500
.set	RSW_INT,		INT_BASE+0x00
.set	DI_INT,			INT_BASE+0x20
.set	SI_INT,			INT_BASE+0x40
.set	EXI_INT,		INT_BASE+0x60

/** Audio streaming **/

.set	REAL_STREAMING,	0xCD80682E

.set	STREAM_BASE,	0xD3026520
.set	STREAM_UPDATE,	STREAM_BASE+0x04
.set	FAKE_STREAMING,	STREAM_BASE+0x20
.set	STREAM_LENGTH,	STREAM_BASE+0x24
.set	STREAM_START,	STREAM_BASE+0x28
.set	STREAM_CURRENT,	STREAM_BASE+0x2C
.set	REALSTREAM_END,	STREAM_BASE+0x30
.set	LOOP_ENABLED,	STREAM_BASE+0x60

/** Disc Interface **/

.set	DI_IMM,		0xC0002F20

.set	DI_BASE,	0xD3026000

/** Triforce **/

.set	IN_TESTMENU,	0xD3003080	/* uncached for kernel */

/** PAD redirection **/

.set	PadStub,	0x93000000
.set	PadBuff,	0x93003100

.set	PadRumble,	0x93003010
.set	PadRumble_0,	PadRumble+0x00
.set	PadRumble_1,	PadRumble+0x04
.set	PadRumble_2,	PadRumble+0x08
.set	PadRumble_3,	PadRumble+0x0C

#endif /* __NINTENDONT_KERNEL_ASM_CONSTANTS_H__ */
