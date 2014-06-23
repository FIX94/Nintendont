#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__	1

#include "global.h"
#include "ipc.h"

u32 thread_create( u32 (*proc)(void* arg), void* arg, u32* stack, u32 stacksize, u8 priority, bool detach );

#define thread_cancel(a,b) syscall_02(a,b)
int syscall_02( int ThreadID, int when);

#define thread_get_id() syscall_03()
u32 syscall_03(void);

#define GetPID( void ) syscall_04( void )
u32 syscall_04( void );

#define thread_continue( ThreadID ) syscall_05( ThreadID )
s32 syscall_05( s32 ThreadID );

#define thread_yield() syscall_03()
u32 syscall_03(void);

#define thread_set_priority(a,b) syscall_09(a,b)
int syscall_09( int ThreadID, int prio);

#define mqueue_create(a, b) syscall_0a(a, b)
int syscall_0a(void *ptr, int n);

#define mqueue_destroy(a) syscall_0b(a)
void syscall_0b(int queue);

#define mqueue_send_now(a, b, c) syscall_0d(a, b, c)
int syscall_0d(int queue, struct ipcmessage *message, int flags);

#define mqueue_recv(a, b, c) syscall_0e(a, b, c)
int syscall_0e(int queue, struct ipcmessage **message, int flags);

int  TimerCreate(int Time, int Dummy, int MessageQueue, int Message );
void TimerDestroy( int TimerID );

#define heap_alloc(a, b) syscall_18(a, b)
void *syscall_18(int heap, int size);

void *heap_alloc_aligned(int heap, int size, int align);

#define heap_free(a, b) syscall_1a(a, b)
void syscall_1a(int, void *);

#define device_register(a, b) syscall_1b(a, b)
int syscall_1b(const char *device, int queue);

s32 IOS_Open(const char *device, int mode);
void IOS_Close(int fd);
s32 IOS_Read(s32 fd,void *buffer,u32 length);
s32 IOS_Write( s32 fd, void *buffer, u32 length );
s32 IOS_Seek( u32 fd, u32 where, u32 whence );

s32 IOS_Ioctl(s32 fd, s32 request, void *buffer_in, s32 length_in, void *buffer_io, s32 length_io);
s32 IOS_Ioctlv(s32 fd, s32 request, s32 InCount, s32 OutCont, void *vec);
s32 IOS_IoctlAsync(s32 fd, s32 request, void *buffer_in, s32 length_in, void *buffer_io, s32 length_io, int queue, struct ipcmessage *message);

#define mqueue_ack(a, b) syscall_2a(a, b)
void syscall_2a(struct ipcmessage *message, int retval);

#define SetUID(PID, b) syscall_2b(PID, b)
u32 syscall_2b( u32 PID, u32 b);

#define SetGID(a,b) syscall_2d(a,b)
u32 syscall_2d( u32 pid, u32 b);

#define cc_ahbMemFlush(a) syscall_2f(a)
void syscall_2f( int device );

#define _ahbMemFlush(a) syscall_30(a)
void syscall_30(int device );

#define irq_enable(a) syscall_34(a)
int syscall_34(int device);

#define sync_before_read(a, b) sync_before_read(a, b)
void sync_before_read(void *ptr, int len);

void sync_after_write(void *ptr, int len);

#define PPCBoot( Path ) syscall_41( Path )
s32 syscall_41( char *Path );

#define IOSBoot( Path, unk, Version ) syscall_42( Path, unk, Version )
s32 syscall_42( char *Path, u32 Flag, u32 Version );

s32 syscall_43( u32 Offset, u32 Version );

#define KernelSetVersion( Version ) syscall_4c( Version )
s32 syscall_4c( u32 Version );

#define KernelGetVersion( void ) syscall_4d( void )
s32 syscall_4d(void);

void *VirtualToPhysical(void *ptr);

#define EnableAHBProt(a) syscall_54(a)
void syscall_54( u32 a );

#define LoadPPC( TMD ) syscall_59( TMD )
s32 syscall_59( u8 *TMD );

#define LoadModule( Path ) syscall_5a( Path )
s32 syscall_5a( char *Path );


s32 sha1( void *SHACarry, void* data, u32 len, u32 SHAMode, void *hash);

#endif
