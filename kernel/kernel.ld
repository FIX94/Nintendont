OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

__stack_size = 0x2000;

MEMORY {

	code		: ORIGIN = 0x20F00000,	LENGTH = 0x20000
	data		: ORIGIN = 0x20F20000,	LENGTH = 0x60000
	stack		: ORIGIN = 0x20106000,	LENGTH = 0x09000
}

PHDRS
{
	code  PT_LOAD AT ( 0x12F00000 );
	data  PT_LOAD AT ( 0x12F20000 );
	stack PT_LOAD AT ( 0x12F86000 );
}

SECTIONS
{
	.init (0x20F00000) : AT( 0x12F00000 )
	{
		*(.init)
		. = ALIGN(4);
	} >code :code

	.text (0x20F00000 + SIZEOF(.init)) : AT( 0x12F00000 + SIZEOF(.init) )
	{
		*(.text*)
		*(.text.*)
		*(.gnu.warning)
		*(.gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} >code :code

	.rodata (0x20F20000) : AT( 0x12F20000 )
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		. = ALIGN(4);
	} >data :data

	.data (0x20F20000 + SIZEOF(.rodata)) : AT( 0x12F20000 + SIZEOF(.rodata) )
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} >data :data

	.bss (0x20F20000 + SIZEOF(.rodata) + SIZEOF(.data)) : AT( 0x12F20000 + SIZEOF(.rodata) + SIZEOF(.data) )
	{
		__bss_start = . ;
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	} >data :data

	.stack (0x20106000) : AT( 0x12F86000 )
	{
		__stack_end = .;
		. = . +__stack_size;
		. = ALIGN(64);
		__stack_addr = .;
	} >stack :stack

}

