/* ioapi_ff.h -- IO base function header for compress/uncompress .zip
   Nintendont: Reworked to use FatFS instead of stdio.
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

         For more info read MiniZip_info.txt

         Changes

    Oct-2009 - Defined ZPOS64_T to fpos_t on windows and u_int64_t on linux. (might need to find a better why for this)
    Oct-2009 - Change to fseeko64, ftello64 and fopen64 so large files would work on linux.
               More if/def section may be needed to support other platforms
    Oct-2009 - Defined fxxxx64 calls to normal fopen/ftell/fseek so they would compile on windows.
                          (but you should use iowin32.c for windows instead)

*/

#ifndef _ZLIBIOAPIFF_H
#define _ZLIBIOAPIFF_H

#include "ioapi.h"

void fill_ff_filefunc OF((zlib_filefunc_def* pzlib_filefunc_def));

#ifdef __cplusplus
}
#endif

#endif
