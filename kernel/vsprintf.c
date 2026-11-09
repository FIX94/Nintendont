/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include "vsprintf.h"
#include "Config.h"
#include "ff_utf8.h"
#include "DI.h"

static FIL dbgfile;
static int file_opened = -1;

/*
 * FatFs is not re-entrant here (_FS_REENTRANT 0), and DIReadThread reads the game
 * through it while the main loop runs. So the log does what GCNCard_Save does:
 * it touches FatFs only while no disc read is in flight, and lines written in the
 * meantime wait in LogQueue for LogFlush() in the main loop. The DI thread is the
 * FatFs user while it runs, so it writes its own lines directly.
 */
static char LogQueue[0x1000];
static u32 LogQueued = 0;
static bool LogHeaderDone = false;
static u32 LogDropped = 0;
static u32 LogSyncTimer = 0;
extern char __di_stack_addr, __di_stack_size;

static bool InDIThread(void)
{
	u32 sp = (u32)&sp;
	u32 top = (u32)&__di_stack_addr, size = (u32)&__di_stack_size;
	return sp - (top - size) < size;
}

static void LogWrite(const char *s, u32 len)
{
	UINT done;
	if (file_opened != FR_OK)
	{
		file_opened = f_open_char(&dbgfile, "/ndebug.log", FA_OPEN_ALWAYS|FA_WRITE);
		if (file_opened != FR_OK)
			return;
		f_lseek(&dbgfile, dbgfile.obj.objsize);
		if (!LogHeaderDone)	// once per boot, not on a reopen
		{
			char hdr[0x80];
			u32 v = read32(0x3140);
			int n = _sprintf(hdr, "Nintendont IOS%d v%d.%d\r\nBuilt   : %s %s\r\nVersion : %d.%d\r\n",
					 v >> 16, (v >> 8) & 0xff, v & 0xff, __DATE__, __TIME__,
					 NIN_VERSION>>16, NIN_VERSION&0xFFFF);
			f_write(&dbgfile, hdr, n, &done);
			LogHeaderDone = true;
		}
	}
	FRESULT res = f_write(&dbgfile, s, len, &done);
	// Each f_sync writes the data sector and the directory entry: do it at most
	// once a second, so the main loop is not held up for every line. A hard
	// hang can lose the last second of the log.
	if (res == FR_OK && TimerDiffSeconds(LogSyncTimer) >= 1)
	{
		res = f_sync(&dbgfile);
		LogSyncTimer = read32(HW_TIMER);
	}
	if (res != FR_OK)
	{
		// After one failed call FatFs refuses every later one on this file
		// object: close it, and the next line opens the log again.
		f_close(&dbgfile);
		file_opened = -1;
	}
}

/**
 * Write the queued log lines, unless a disc read is in flight.
 * Called by the main loop every pass.
 */
void LogFlush(void)
{
	if (LogQueued == 0 || DIReadPending())
		return;
	LogWrite(LogQueue, LogQueued);
	LogQueued = 0;
	if (LogDropped)
	{
		char note[0x30];
		LogWrite(note, _sprintf(note, "[log: %u lines dropped]\r\n", LogDropped));
		LogDropped = 0;
	}
}
vu32 SDisInit=0;

extern int svc_write(char *buffer);

static inline int isdigit(int c)
{
	return c >= '0' && c <= '9';
}

static inline int isxdigit(int c)
{
	return (c >= '0' && c <= '9')
	    || (c >= 'a' && c <= 'f')
	    || (c >= 'A' && c <= 'F');
}

static inline int islower(int c)
{
	return c >= 'a' && c <= 'z';
}

static inline int toupper(int c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}

static int skip_atoi(const char **s)
{
	int i=0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })

static char * number(char * str, long num, int base, int size, int precision
	,int type)
{
	char c,sign,tmp[66];
	const char *digits="0123456789abcdefghijklmnopqrstuvwxyz";
	int i;

	if (type & LARGE)
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num != 0)
		tmp[i++] = digits[do_div(num,base)];
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))
		while(size-->0)
			*str++ = ' ';
	if (sign)
		*str++ = sign;
	if (type & SPECIAL) {
		if (base==8)
			*str++ = '0';
		else if (base==16) {
			*str++ = '0';
			*str++ = digits[33];
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			*str++ = c;
	while (i < precision--)
		*str++ = '0';
	while (i-- > 0)
		*str++ = tmp[i];
	while (size-- > 0)
		*str++ = ' ';
	return str;
}

int _vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	unsigned long num;
	int i, base;
	char * str;
	const char *s;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for (str=buf ; *fmt ; ++fmt) {
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}

		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		/* default base */
		base = 10;

		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char) va_arg(args, int);
			while (--field_width > 0)
				*str++ = ' ';
			continue;

		case 's':
			s = va_arg(args, char *);
			//if (!s)
			//	s = "<NULL>";

			len = strnlen(s, precision);

			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)
				*str++ = *s++;
			while (len < field_width--)
				*str++ = ' ';
			continue;

		case 'p':
			if (field_width == -1) {
				field_width = 2*sizeof(void *);
				flags |= ZEROPAD;
			}
			str = number(str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			continue;


		case 'n':
			if (qualifier == 'l') {
				long * ip = va_arg(args, long *);
				*ip = (str - buf);
			} else {
				int * ip = va_arg(args, int *);
				*ip = (str - buf);
			}
			continue;

		case '%':
			*str++ = '%';
			continue;

		/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;

		case 'X':
			flags |= LARGE;
		case 'x':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			*str++ = '%';
			if (*fmt)
				*str++ = *fmt;
			else
				--fmt;
			continue;
		}
		if (qualifier == 'l')
			num = va_arg(args, unsigned long);
		else if (qualifier == 'h') {
			num = (unsigned short) va_arg(args, int);
			if (flags & SIGN)
				num = (short) num;
		} else if (flags & SIGN)
			num = va_arg(args, int);
		else
			num = va_arg(args, unsigned int);
		str = number(str, num, base, field_width, precision, flags);
	}
	*str = '\0';
	return str-buf;
}

int dbgprintf( const char *fmt, ...)
{
	va_list args;

	if ( (*(vu32*)(0xd800070) & 1) == 0)
		return -1;
	
	//char *buffer = (char*)heap_alloc_aligned( 0, 2048, 32 );	
	char buffer[0x100]; //get from stack

	va_start(args, fmt);
	_vsprintf(buffer, fmt, args);
	va_end(args);

	if( SDisInit )
	{
		u32 len = strlen(buffer);
		if (InDIThread())
			LogWrite(buffer, len);
		else
		{
			// Full during one long read: drop the line rather than wait, since
			// DIFinishAsync() logs the game's OSReport lines through here.
			if (LogQueued + len > sizeof(LogQueue))
				LogDropped++;
			else
			{
				memcpy(LogQueue + LogQueued, buffer, len);
				LogQueued += len;
			}
			LogFlush();
		}
	}

	if( !IsWiiU() ) // usbgecko?
		svc_write(buffer);

	//heap_free( 0, buffer );

	return 0;
}
void closeLog(void)
{
	DIFinishAsync();
	LogFlush();
	if(file_opened == FR_OK)
	{
		file_opened = -1;
		f_close(&dbgfile);
	}
}
void CheckOSReport(void)
{
	sync_before_read((void*)0x13160000, 0x8);

	u32 Length = read32(0x13160004);
	if (Length != 0x0)
	{
		if (Length > 0xFF)	// dbgprintf's buffer is 0x100
			Length = 0xFF;
		char* Address = (char*)(P2C(read32(0x13160000)));
		sync_before_read((void*)((u32)(Address) & 0x1FFFFFFC), Length + 4);
		char* Msg = malloca(Length + 1, 0x20);
		strncpy(Msg, Address, Length);
		Msg[Length] = '\0';
		dbgprintf("%s", Msg);	// the game's text, not a format
		free(Msg);
		write32(0x13160004, 0);
		sync_after_write((void*)0x13160004, 0x4);
	}
	return;
}
