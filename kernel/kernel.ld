OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

__stack_size = 32;

MEMORY {

	code		: ORIGIN = 0x20100000,	LENGTH = 0xD000
	data		: ORIGIN = 0x2010D000,	LENGTH = 0x1000
	bss			: ORIGIN = 0x2010E000,	LENGTH = 0x46A8
}

PHDRS
{
	code PT_LOAD AT ( 0x139F0000 );
	data PT_LOAD AT ( 0x139FD000 );
	bss  PT_LOAD AT ( 0x139FE000 );
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

	.data (0x2010D000) : AT( 0x139FD000 )
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} >data :data

	.bss (0x2010E000) : AT( 0x139FE000 )
	{
		__bss_start = . ;
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	} >bss :bss

	.stack (0x2010E000 + SIZEOF(.bss)) : AT( 0x139FE000 + SIZEOF(.bss) )
	{
		__stack_end = .;
		. = . +__stack_size;
		. = ALIGN(64);
		__stack_addr = .;
	} >bss :bss

}

