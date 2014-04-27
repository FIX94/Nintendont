#ifndef __USBGECKO_H__
#define __USBGECKO_H__

#define USBGECKO_CMD_IDENTIFY		0x90000000
#define USBGECKO_IDENTIFY_RESPONSE	0x04700000
#define USBGECKO_CMD_TX_STATUS		0xc0000000
#define USBGECKO_TX_FIFO_READY		0x04000000
#define USBGECKO_CMD_RX_STATUS		0xd0000000
#define USBGECKO_RX_FIFO_READY		0x04000000
#define USBGECKO_CMD_RECEIVE		0xa0000000 
#define USBGECKO_CMD_TRANSMIT(ch)	(0xb0000000 | ((ch) << 20))

void usbgecko_putch(char ch);
void usbgecko_printf(const char* string);
void usbgecko_init();

#endif
