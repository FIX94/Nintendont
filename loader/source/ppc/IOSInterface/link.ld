
OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)

ENTRY(__start)

SECTIONS {
	/* placed in by kernel Patch.c */
	OSSleepThread = 0x93026C84;

	. = 0x93006000;

	.start : { _crt0.o(*) }
	.text : { *(.text) }
	.rodata : { *(.rodata .rodata.*)}
	.data : { *(.data) }

	__bss_start = .;
	.bss : { *(.bss) }
	__bss_end = .;
}
