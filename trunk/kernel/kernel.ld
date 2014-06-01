OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

__stack_size = 32;

MEMORY {

	code		: ORIGIN = 0x20100000,	LENGTH = 0x10000
	data		: ORIGIN = 0x20114000,	LENGTH = 0x02000
	bss			: ORIGIN = 0x20116000,	LENGTH = 0x0A000
}

PHDRS
{
	code PT_LOAD AT ( 0x139F0000 );
	data PT_LOAD AT ( 0x13A04000 );
	bss  PT_LOAD AT ( 0x13A06000 );
}

SECTIONS
{
	.init (0x20100000) : AT( 0x139F0000 )
	{
		*(.init)
		. = ALIGN(4);
	} >code :code

	.text (0x20100000 + SIZEOF(.init)) : AT( 0x139F0000 + SIZEOF(.init) )
	{
		*(.text*)
		*(.text.*)
		*(.gnu.warning)
		*(.gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} >code :code

	.rodata (0x20100000 + SIZEOF(.init) + SIZEOF(.text)) : AT( 0x139F0000 + SIZEOF(.init) + SIZEOF(.text) )
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		. = ALIGN(4);
	} >code :code

	.data (0x20114000) : AT( 0x13A04000 )
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} >data :data

	.bss (0x20116000) : AT( 0x13A06000 )
	{
		__bss_start = . ;
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	} >bss :bss

	.stack (0x20116000 + SIZEOF(.bss)) : AT( 0x13A06000 + SIZEOF(.bss) )
	{
		__stack_end = .;
		. = . +__stack_size;
		. = ALIGN(64);
		__stack_addr = .;
	} >bss :bss

}

