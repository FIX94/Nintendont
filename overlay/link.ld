OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)
ENTRY(overlay_entry)
SECTIONS {
 . = 0x93180000;
 .text : { KEEP(*(.entry)) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 ASSERT(. <= 0x9318C000, "Overlay exceeds reserved code region")
 /DISCARD/ : { *(.comment*) *(.eh_frame*) *(.note*) }
}
