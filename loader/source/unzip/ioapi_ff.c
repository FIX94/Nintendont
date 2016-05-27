/* ioapi_ff.h -- IO base function header for compress/uncompress .zip
   Nintendont: Reworked to use FatFS instead of stdio.
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

         For more info read MiniZip_info.txt

*/

#include "zlib.h"
#include "ioapi.h"
#include "ff_utf8.h"

#include <stdlib.h>

static voidpf ZCALLBACK fopen_ff_func(voidpf opaque, const char* filename, int mode)
{
	FIL* file = NULL;
	BYTE mode_fopen = 0; 
	// TODO: Verify these mode flags.
	if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
		mode_fopen = FA_READ|FA_OPEN_EXISTING;
	else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
		mode_fopen = FA_WRITE|FA_OPEN_EXISTING;
	else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
		mode_fopen = FA_WRITE|FA_CREATE_NEW;

	if (filename != NULL && mode_fopen != 0) {
		file = malloc(sizeof(FIL));
		if (f_open_char(file, filename, mode_fopen) != FR_OK) {
			free(file);
			file = NULL;
		}
	}
	return file;
}

static uLong ZCALLBACK fread_ff_func(voidpf opaque, voidpf stream, void* buf, uLong size)
{
	UINT read;
	f_read((FIL*)stream, buf, size, &read);
	return read;
}

static uLong ZCALLBACK fwrite_ff_func(voidpf opaque, voidpf stream, const void* buf, uLong size)
{
	UINT wrote;
	f_write((FIL*)stream, buf, size, &wrote);
	return wrote;
}

static long ZCALLBACK ftell_ff_func(voidpf opaque, voidpf stream)
{
	return f_tell((FIL*)stream);
}

static long ZCALLBACK fseek_ff_func(voidpf opaque, voidpf stream, uLong offset, int origin)
{
	// FatFS doesn't have an origin parameter,
	// so we'll implement it here.
	FSIZE_t pos = offset;
	switch (origin) {
		case ZLIB_FILEFUNC_SEEK_CUR:
			pos += f_tell((FIL*)stream);
			break;
		case ZLIB_FILEFUNC_SEEK_END:
			pos += ((FIL*)stream)->obj.objsize + offset;
			break;
		case ZLIB_FILEFUNC_SEEK_SET:
			// Nothing to change here...
			break;
		default:
			return -1;
	}

	if (f_lseek((FIL*)stream, pos) != FR_OK)
		return -1;
	return 0;
}

static int ZCALLBACK fclose_ff_func(voidpf opaque, voidpf stream)
{
	return f_close((FIL*)stream);
}

static int ZCALLBACK ferror_ff_func (voidpf opaque, voidpf stream)
{
	return f_error((FIL*)stream);
}

// TODO: Add a 64-bit version when enabling exFAT?
void fill_ff_filefunc(zlib_filefunc_def* pzlib_filefunc_def)
{
	pzlib_filefunc_def->zopen_file = fopen_ff_func;
	pzlib_filefunc_def->zread_file = fread_ff_func;
	pzlib_filefunc_def->zwrite_file = fwrite_ff_func;
	pzlib_filefunc_def->ztell_file = ftell_ff_func;
	pzlib_filefunc_def->zseek_file = fseek_ff_func;
	pzlib_filefunc_def->zclose_file = fclose_ff_func;
	pzlib_filefunc_def->zerror_file = ferror_ff_func;
	pzlib_filefunc_def->opaque = NULL;
}
