
	.section ".text"
	.align	4
	.arm
	.global thread_create
	.type   thread_create STT_FUNC
thread_create:
	.long 0xe6000010
	bx lr

	.global thread_continue
	.type   thread_continue STT_FUNC
thread_continue:
	.long 0xe60000b0
	bx lr

	.global sync_before_read
	.type   sync_before_read STT_FUNC
sync_before_read:
	.long 0xe60007f0
	bx lr

	.global sync_after_write
	.type   sync_after_write STT_FUNC
sync_after_write:
	.long 0xe6000810
	bx lr

	.pool
	.end
