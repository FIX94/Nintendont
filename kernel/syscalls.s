	.section ".text"
	.align	4
	.arm
	.global thread_create
	.type   thread_create STT_FUNC
thread_create:
	.long 0xe6000010
	bx lr

	.global syscall_02
	.type   syscall_02 STT_FUNC
syscall_02:
	.long 0xe6000050
	bx lr

	.global syscall_03
	.type   syscall_03 STT_FUNC
syscall_03:
	.long 0xe6000070
	bx lr

	.global syscall_04
	.type   syscall_04 STT_FUNC
syscall_04:
	.long 0xe6000090
	bx lr

	.global syscall_05
	.type   syscall_05 STT_FUNC
syscall_05:
	.long 0xe60000b0
	bx lr

	.global syscall_07
	.type   syscall_07 STT_FUNC
syscall_07:
	.long 0xe60000f0
	bx lr

	.global syscall_09
	.type   syscall_09 STT_FUNC
syscall_09:
	.long 0xe6000130
	bx lr

	.global syscall_0a
	.type   syscall_0a STT_FUNC
syscall_0a:
	.long 0xe6000150
	bx lr

	.global syscall_0b
	.type   syscall_0b STT_FUNC
syscall_0b:
	.long 0xe6000170
	bx lr

	.global syscall_0e
	.type   syscall_0e STT_FUNC
syscall_0e:
	.long 0xe60001d0
	bx lr

	.global TimerCreate
	.type   TimerCreate STT_FUNC
TimerCreate:
	.long 0xe6000230
	bx lr

	.global TimerDestroy
	.type   TimerDestroy STT_FUNC
TimerDestroy:
	.long 0xe6000290
	bx lr

	.global syscall_18
	.type   syscall_18 STT_FUNC
syscall_18:
	.long 0xe6000310
	bx lr

	.global heap_alloc_aligned
	.type   heap_alloc_aligned STT_FUNC
heap_alloc_aligned:
	.long 0xe6000330
	bx lr

	.global syscall_1a
	.type   syscall_1a STT_FUNC
syscall_1a:
	.long 0xe6000350
	bx lr

	.global syscall_1b
	.type   syscall_1b STT_FUNC
syscall_1b:
	.long 0xe6000370
	bx lr

	.global IOS_Open
	.type   IOS_Open STT_FUNC
IOS_Open:
	.long 0xe6000390
	bx lr

	.global IOS_Close
	.type   IOS_Close STT_FUNC
IOS_Close:
	.long 0xe60003b0
	bx lr

	.global IOS_Read
	.type   IOS_Read STT_FUNC
IOS_Read:
	.long 0xe60003d0
	bx lr

	.global IOS_Write
	.type   IOS_Write STT_FUNC
IOS_Write:
	.long 0xe60003f0
	bx lr

	.global IOS_Seek
	.type   IOS_Seek STT_FUNC
IOS_Seek:
	.long 0xe6000410
	bx lr

	.global IOS_Ioctl
	.type   IOS_Ioctl STT_FUNC
IOS_Ioctl:
	.long 0xe6000430
	bx lr

	.global IOS_Ioctlv
	.type   IOS_Ioctlv STT_FUNC
IOS_Ioctlv:
	.long 0xe6000450
	bx lr

	.global IOS_IoctlAsync
	.type   IOS_IoctlAsync STT_FUNC
IOS_IoctlAsync:
	.long 0xe6000510
	bx lr
	
	.global syscall_2b
	.type   syscall_2b STT_FUNC
syscall_2b:
	.long 0xe6000570
	bx lr

	.global syscall_2d
	.type   syscall_2d STT_FUNC
syscall_2d:
	.long 0xe60005b0
	bx lr

	.global syscall_2f
	.type   syscall_2f STT_FUNC
syscall_2f:
	.long 0xe60005f0
	bx lr

	.global syscall_30
	.type   syscall_30 STT_FUNC
syscall_30:
	.long 0xe6000610
	bx lr

	.global syscall_34
	.type   syscall_34 STT_FUNC
syscall_34:
	.long 0xe6000690
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

	.global syscall_41
	.type   syscall_41 STT_FUNC
syscall_41:
	.long 0xE6000830
	bx lr

	.global syscall_42
	.type   syscall_42 STT_FUNC
syscall_42:
	.long 0xE6000850
	bx lr

	.global syscall_43
	.type   syscall_43 STT_FUNC
syscall_43:
	.long 0xE6000870
	bx lr

	.global syscall_4c
	.type   syscall_4c STT_FUNC
syscall_4c:
	.long 0xe6000990
	bx lr

	.global syscall_4d
	.type   syscall_4d STT_FUNC
syscall_4d:
	.long 0xe60009b0
	bx lr
	
	.global syscall_54
	.type   syscall_54 STT_FUNC
syscall_54:
	.long 0xe6000a90
	bx lr
	
	.global VirtualToPhysical
	.type   VirtualToPhysical STT_FUNC
VirtualToPhysical:
	.long 0xe60009f0
	bx lr

	.global syscall_59
	.type   syscall_59 STT_FUNC
syscall_59:
	.long 0xe6000b30
	bx lr

	.global syscall_5a
	.type   syscall_5a STT_FUNC
syscall_5a:
	.long 0xe6000b50
	bx lr

	.global sha1
	.type   sha1 STT_FUNC
sha1:
	.long 0xe6000cf0
	bx lr

	.global svc_write
	.type   svc_write STT_FUNC
svc_write:
	mov r1, r0
	mov r0, #4
	svc #0xab
	bx lr

	.pool
	.end
