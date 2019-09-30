
OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)

ENTRY(__start)

SECTIONS {
	.code 0x93000000 :
	{
		_crt0.o(*)
		*(.text)
		*(.rodata .rodata.*)
		*(.data)
		*(.bss)
		. = 0x3000;
	}
}
