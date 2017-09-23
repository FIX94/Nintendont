// Nintendont: FatFs UTF-8 wrapper functions.
#include "ff_utf8.h"
#include <stdlib.h>
#include <stdbool.h>

// Temporary WCHAR buffer.
// NOT REENTRANT OR THREAD-SAFE!
// NOTE: wchar_t is 32-bit; WCHAR is 16-bit.
// Hence, we have to convert between the two
// when using mbstowcs() and related functions.
static union {
       WCHAR u16[1024];
       wchar_t u32[512];
} tmpwchar;

#define NUM_ELEMENTS(x) ((int)(sizeof(x)/sizeof(x[0])))

/**
 * Convert a UTF-8 string to WCHAR (UTF-16).
 * Uses tmpwchar buffer. (NOT REENTRANT OR THREAD-SAFE!)
 * @param str UTF-8 string.
 * @return True if converted; false if string is empty or invalid.
 */
static inline bool char_to_wchar(const char *str)
{
	size_t len, i;
	if (!str || *str == 0)
		return false;

	len = mbstowcs(tmpwchar.u32, str, NUM_ELEMENTS(tmpwchar.u32));
	if (len == 0)
		return false;
	if (len >= NUM_ELEMENTS(tmpwchar.u32))
		len = NUM_ELEMENTS(tmpwchar.u32)-1;

	// Convert from UTF-32 to UTF-16.
	// NOTE: Characters >U+FFFF are not supported.
	for (i = 0; i < len && tmpwchar.u32[i] != 0; ++i) {
		tmpwchar.u16[i] = (WCHAR)(tmpwchar.u32[i] & 0xFFFF);
       }

	// NULL-terminate the string.
	tmpwchar.u16[i] = 0;
	return true;
}

/**
 * Convert a UTF-16 string (WCHAR) to UTF-8.
 * Uses tmpchar and tmpwchar buffers. (NOT REENTRANT OR THREAD-SAFE!)
 * @param wcs WCHAR string.
 * @return UTF-8 string. (STATIC BUFFER; use immediately, DO NOT FREE!)
 */
const char *wchar_to_char(const WCHAR *wcs)
{
	static char tmpchar[512];
	size_t i;
	wchar_t *wcptr = tmpwchar.u32;

	// Convert the WCHAR string to wchar_t.
	// NOTE: Characters >U+FFFF are not supported.
	for (i = 0; i < NUM_ELEMENTS(tmpwchar.u32); i++) {
		if (*wcs == 0)
			break;
		*wcptr++ = *wcs++;
	}

	// NULL-terminate the string.
	*wcptr = 0;

	/// Convert from UTF-32 to UTF-8.
	wcstombs(tmpchar, tmpwchar.u32, NUM_ELEMENTS(tmpchar));
	return tmpchar;
}

FRESULT f_open_char(FIL* fp, const char* path, BYTE mode)
{
	if (!char_to_wchar(path))
		return FR_INVALID_NAME;
	return f_open(fp, tmpwchar.u16, mode);
}

FRESULT f_mount_char(FATFS* fs, const char* path, BYTE opt)
{
	if (!char_to_wchar(path))
		return FR_INVALID_NAME;
	return f_mount(fs, tmpwchar.u16, opt);
}

#if !_FS_READONLY
FRESULT f_mkdir_char(const char* path)
{
	if (!char_to_wchar(path))
		return FR_INVALID_NAME;
	return f_mkdir(tmpwchar.u16);
}
#endif /* !_FS_READONLY */

#if _FS_RPATH >= 1
#if _VOLUMES >= 2
FRESULT f_chdrive_char(const char* path)
{
	if (!char_to_wchar(path))
		return FR_INVALID_NAME;
	return f_chdrive(tmpwchar.u16);
}
#endif /* _VOLUMES >= 2 */

FRESULT f_chdir_char(const char* path)
{
	if (!char_to_wchar(path))
		return FR_INVALID_NAME;
	return f_chdir(tmpwchar.u16);
}
#endif /* _FS_RPATH >= 1 */

#if _FS_MINIMIZE <= 1
FRESULT f_opendir_char(DIR* dp, const char* path)
{
	if (!char_to_wchar(path))
		return FR_INVALID_NAME;
	return f_opendir(dp, tmpwchar.u16);
}
#endif /* _FS_MINIMIZE <= 1 */
