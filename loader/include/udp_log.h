// #include <stdarg.h>
// #include <stdio.h>
// #include <string.h>
// #include <network.h>

// needed??  #include "global.h"
#include <gctypes.h>


#ifndef	DEFAULT_UDP_LOG_PORT
#define DEFAULT_UDP_LOG_PORT  9930
#endif 
#ifndef	DEFAULT_UDP_LOG_HOST
#define	DEFAULT_UDP_LOG_HOST  "192.168.43.132" // Address of debug server
#endif 

int udp_printf_to(const char* ipv4_addr, u16 port, const char* fmt, ...);

#define udp_printf(fmt, ...) udp_printf_to(DEFAULT_UDP_LOG_HOST, DEFAULT_UDP_LOG_PORT, fmt, __VA_ARGS__)


