#include "JVSIOMessage.h"

int m_ptr, m_last_start, m_csum;
unsigned char m_msg[0x80];

void JVSIOMessage(void)
{
	m_ptr = 0;
	m_last_start = 0;
}
void JVSIOstart(int node)
{
	m_last_start = m_ptr;
	unsigned char hdr[3] = {0xe0, node, 0};
	m_csum = 0;
	addData(hdr, 3, 1);
}
void addDataBuffer(const void *data, size_t len)
{
	addData((const unsigned char*)data, len, 0);
}
void addDataString(const char *data)
{
	addDataBuffer(data, strlen(data));
}
void addDataByte(int n)
{
	unsigned char cs = n;
	addDataBuffer(&cs, 1);
}

void end()
{
	int len = m_ptr - m_last_start;
	m_msg[m_last_start + 2] = len - 2; // assuming len <0xD0
	addDataByte(m_csum + len - 2);
}

void addData(const unsigned char *dst, size_t len, int sync )
{
	while (len--)
	{
		int c = *dst++;
		if (!sync && ((c == 0xE0) || (c == 0xD0)))
		{
			m_msg[m_ptr++] = 0xD0;
			m_msg[m_ptr++] = c - 1;
		} else
			m_msg[m_ptr++] = c;
		if (!sync)
			m_csum += c;
		sync = 0;

		//if (m_ptr >= 0x80)
		//	dbgprintf("JVSIOMessage overrun!");
	}
}
