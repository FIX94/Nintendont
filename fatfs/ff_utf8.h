// Nintendont: FatFs UTF-8 wrapper functions.
#ifndef _FATFS_UTF8
#define _FATFS_UTF8

#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"

/**
 * Convert a 16-bit WCHAR string to UTF-8.
 * @param in WCHAR string.
 * @return UTF-8 string. (STATIC BUFFER; use immediately, DO NOT FREE!)
 */
const char *wchar_to_char(const WCHAR *wcs);

FRESULT f_open_char(FIL* fp, const char* path, BYTE mode);
FRESULT f_mount_char(FATFS* fs, const char* path, BYTE opt);

#if !_FS_READONLY
FRESULT f_mkdir_char(const char* path);
#endif /* !_FS_READONLY */

#if _FS_RPATH >= 1
#if _VOLUMES >= 2
FRESULT f_chdrive_char(const char* path);
#endif /* _VOLUMES >= 2 */
FRESULT f_chdir_char(const char* path);
#endif /* _FS_RPATH >= 1 */

#if _FS_MINIMIZE <= 1
FRESULT f_opendir_char(DIR* dp, const char* path);
#endif /* _FS_MINIMIZE <= 1 */

#ifdef __cplusplus
}
#endif

#endif /* _FATFS_UTF8 */
