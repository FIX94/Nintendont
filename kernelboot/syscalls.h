
#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

unsigned int thread_create( unsigned int (*proc)(void* arg), void* arg, unsigned int* stack, unsigned int stacksize, unsigned char priority, char detach );
int thread_continue( int ThreadID );

void sync_before_read(void *ptr, int len);
void sync_after_write(void *ptr, int len);

#endif
