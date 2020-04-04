#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <network.h>
#include "udp_log.h"

static u8 __udp_log__init_network = 1;

//---------------------------------------------------------------------------------
int udp_printf_to(const char* ipv4_addr, u16 port, const char* fmt, ...){
//  return 0 if success
//  return 1 if fail to create socket
//  return 2 if fail to send bytes 
//  return 3 if fail to init network 
//---------------------------------------------------------------------------------
	s32 sock;
	struct sockaddr_in sa;
	int bytes_sent;
	char buffer[200];
	// assemble message
	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(buffer, 200, fmt, argptr);
	va_end(argptr);

	// network init on first time ; can take ~5 seconds 
	if(__udp_log__init_network == 1){
		printf("Configuring network ...\n");
		// // Configure the network interface
		char localip[16] = {0};
		char gateway[16] = {0};
		char netmask[16] = {0};
		if (if_config ( localip, netmask, gateway, TRUE, 20) == 0) {
			printf ("network configured, ip: %s, gw: %s, mask %s\n", localip, gateway, netmask);
			__udp_log__init_network = 0;
		} else {
			printf ("network configuration failed!\n");
			__udp_log__init_network = 2;
		}
	}

	if(__udp_log__init_network == 0){
		/* create an Internet, datagram, socket using UDP */
		sock = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (sock == -1) {
			/* if socket failed to initialize, exit */
			printf("Error Creating Socket\n");
			return 1;
		}

		/* Zero out socket address */
		memset(&sa, 0, sizeof sa);

		/* The address is IPv4 */
		sa.sin_family = AF_INET;

		/* IPv4 adresses is a uint32_t, convert a string representation of the octets to the appropriate value */
		sa.sin_addr.s_addr = inet_addr(ipv4_addr);

		/* sockets are unsigned shorts, htons(x) ensures x is in network byte order, set the port to 7654 */
		sa.sin_port = htons(port);
		sa.sin_len = 8;

		
		int ret = 0;
		// printf("Sending UDP packet @ %s:%d\n",ipv4_addr, port);	
		bytes_sent = net_sendto(sock, buffer, strlen(buffer), 0,(struct sockaddr*)&sa, 8);
		if (bytes_sent < 0) {
			printf("Error sending packet: code[%d]\n", bytes_sent);
			ret = 2;
		}
		net_close(sock); /* close the socket */

		return ret; // 0 if all good, else 2
	}
	return 3;  // network init fail
}

