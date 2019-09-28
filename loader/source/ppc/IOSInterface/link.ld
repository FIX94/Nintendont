
OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)

ENTRY(__start)

SECTIONS {
	/* placed in by kernel Patch.c */
	OSSleepThread = 0x93026C84;
	OSWakeupThread = 0x93026C88;
	OSCreateThread = 0x93026C8C;
	OSResumeThread = 0x93026C90;
	DHCPStateOffset = 0x93026C94;
	SOStartedOffset = 0x93026C96;
	SOShift = 0x93026C98;
	SOCurrentTotalFDs = 0x93026C9A;
	IsInited = 0x93026C9C;

	. = 0x93006000;

	.start : { _crt0.o(*) }
	.text : { *(.text) }
	.rodata : { *(.rodata .rodata.*)}
	.data : { *(.data) }

	__bss_start = .;
	.bss : { *(.bss) }
	__bss_end = .;
}
