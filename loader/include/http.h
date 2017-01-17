#ifndef _HTTP_H_
#define _HTTP_H_

#include <gctypes.h>
#define TCP_CONNECT_TIMEOUT 5000
#define TCP_BLOCK_SIZE (16 * 1024)
#define TCP_BLOCK_RECV_TIMEOUT 4000
#define TCP_BLOCK_SEND_TIMEOUT 4000

s32 tcp_socket (void);
s32 tcp_connect (char *host, const u16 port);

char * tcp_readln (const s32 s, const u16 max_length, const u64 start_time, const u16 timeout);
bool tcp_read (const s32 s, u8 **buffer, const u32 length);
bool tcp_write (const s32 s, const u8 *buffer, const u32 length);

#define HTTP_TIMEOUT 300000

typedef enum {
	HTTPR_OK,
	HTTPR_ERR_CONNECT,
	HTTPR_ERR_REQUEST,
	HTTPR_ERR_STATUS,
	HTTPR_ERR_TOOBIG,
	HTTPR_ERR_RECEIVE
} http_res;

bool http_request (const char *url, const u32 max_size);
bool http_post (const char *url, const u32 max_size, const char *postData);
bool http_get_result (unsigned int *http_status, u8 **content, unsigned int *length);

#endif

