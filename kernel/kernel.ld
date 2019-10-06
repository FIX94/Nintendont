OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

__stack_size = 0x2000;
__phy_stack_addr = 0x12FE0000 + __stack_size;

__di_stack_size = 0x1000;
__di_stack_addr = __phy_stack_addr + __di_stack_size;

__realdi_stack_size = 0x400;
__realdi_stack_addr = __di_stack_addr + __realdi_stack_size;

__hid_stack_size = 0x400;
__hid_stack_addr = __realdi_stack_addr + __hid_stack_size;

__intr_stack_size = 0x400;
__intr_stack_addr = __hid_stack_addr + __intr_stack_size;

__blk_stack_size = 0x400;
__blk_stack_addr = __intr_stack_addr + __blk_stack_size;

MEMORY {

	code		: ORIGIN = 0x12F00000,	LENGTH = 0x60000
	data		: ORIGIN = 0x12F60000,	LENGTH = 0x80000
	stack		: ORIGIN = 0x12FE0000,	LENGTH = 0x10000
}

PHDRS
{
	code  PT_LOAD AT ( 0x12F00000 );
	data  PT_LOAD AT ( 0x12F60000 );
	stack PT_LOAD AT ( 0x12FE0000 );
}

SECTIONS
{
	.init (0x12F00000) : AT( 0x12F00000 )
	{
		*(.init)
		. = ALIGN(4);
	} >code :code

	.text (0x12F00000 + SIZEOF(.init)) : AT( 0x12F00000 + SIZEOF(.init) )
	{
		*(.text*)
		*(.text.*)
		*(.gnu.warning)
		*(.gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} >code :code

	.rodata (0x12F60000) : AT( 0x12F60000 )
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		. = ALIGN(4);
	} >data :data

	.data (0x12F60000 + SIZEOF(.rodata)) : AT( 0x12F60000 + SIZEOF(.rodata) )
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} >data :data

	.bss (0x12F60000 + SIZEOF(.rodata) + SIZEOF(.data)) : AT( 0x12F60000 + SIZEOF(.rodata) + SIZEOF(.data) )
	{
		__bss_start = . ;
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	} >data :data

	.stack (0x12FE0000) : AT( 0x12FE0000 )
	{
		__stack_end = .;
		. = . +__stack_size;
		. = ALIGN(64);
		__stack_addr = .;
	} >stack :stack

}

