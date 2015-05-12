/* Code taken from http://wiibrew.org/wiki//dev/net/ssl/code */

#ifndef _SSL_H_
#define _SSL_H_

#define IOCTLV_SSL_NEW 1
#define IOCTLV_SSL_CONNECT 2
#define IOCTLV_SSL_HANDSHAKE 3
#define IOCTLV_SSL_READ 4
#define IOCTLV_SSL_WRITE 5
#define IOCTLV_SSL_SHUTDOWN 6
#define IOCTLV_SSL_SETROOTCA 10
#define IOCTLV_SSL_SETBUILTINCLIENTCERT 14

#define SSL_HEAP_SIZE 0xB000

u32 ssl_init(void);
u32 ssl_open(void);
u32 ssl_close(void);
s32 ssl_new(u8 * CN, u32 verify_options);
s32 ssl_setbuiltinclientcert(s32 ssl_context, s32 index);
s32 ssl_setrootca(s32 ssl_context, const void *root, u32 length);
s32 ssl_connect(s32 ssl_context, s32 socket);
s32 ssl_handshake(s32 ssl_context);
s32 ssl_read(s32 ssl_context, void* buffer, u32 length);
s32 ssl_write(s32 ssl_context, const void * buffer, u32 length);
s32 ssl_shutdown(s32 ssl_context);

#endif
