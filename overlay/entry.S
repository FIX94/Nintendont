.section .entry,"ax"
.globl overlay_entry
overlay_entry:
	b menu_input
	b menu_draw
