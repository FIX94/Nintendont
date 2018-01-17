/* ioapi_mem.c -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API
   This version of ioapi is designed to access memory rather than files.
   We do use a region of memory to put data in to and take it out of. We do
   not have auto-extending buffers and do not inform anyone else that the
   data has been written. It is really intended for accessing a zip archive
   embedded in an application such that I can write an installer with no
   external files. Creation of archives has not been attempted, although
   parts of the framework are present.
   Based on Unzip ioapi.c version 0.22, May 19th, 2003
   Copyright (C) 1998-2003 Gilles Vollant
             (C) 2003 Justin Fletcher
   This file is under the same license as the Unzip tool it is distributed
   with.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"
#include "ioapi.h"



voidpf ZCALLBACK fopen_mem_func OF((
   voidpf opaque,
   const char* filename,
   int mode));

uLong ZCALLBACK fread_mem_func OF((
   voidpf opaque,
   voidpf stream,
   void* buf,
   uLong size));

uLong ZCALLBACK fwrite_mem_func OF((
   voidpf opaque,
   voidpf stream,
   const void* buf,
   uLong size));

long ZCALLBACK ftell_mem_func OF((
   voidpf opaque,
   voidpf stream));

long ZCALLBACK fseek_mem_func OF((
   voidpf opaque,
   voidpf stream,
   uLong offset,
   int origin));

int ZCALLBACK fclose_mem_func OF((
   voidpf opaque,
   voidpf stream));

int ZCALLBACK ferror_mem_func OF((
   voidpf opaque,
   voidpf stream));


typedef struct ourmemory_s {
  char *base; /* Base of the region of memory we're using */
  uLong size; /* Size of the region of memory we're using */
  uLong limit; /* Furthest we've written */
  uLong cur_offset; /* Current offset in the area */
} ourmemory_t;

voidpf ZCALLBACK fopen_mem_func (opaque, filename, mode)
   voidpf opaque;
   const char* filename;
   int mode;
{
    ourmemory_t *mem = malloc(sizeof(*mem));
    if (mem==NULL)
      return NULL; /* Can't allocate space, so failed */

    /* Filenames are specified in the form :
     *    <hex base of zip file>+<hex size of zip file>
     * This may not work where memory addresses are longer than the
     * size of an int and therefore may need addressing for 64bit
     * architectures
     */
    if (sscanf(filename,"%x+%x",(unsigned int*)&mem->base,(unsigned int*)&mem->size)!=2)
    {
      free(mem);
      return NULL;
    }

    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
      mem->limit=0; /* When writing we start with 0 bytes written */
    else
      mem->limit=mem->size;

    mem->cur_offset = 0;

    return mem;
}


uLong ZCALLBACK fread_mem_func (opaque, stream, buf, size)
   voidpf opaque;
   voidpf stream;
   void* buf;
   uLong size;
{
    ourmemory_t *mem = (ourmemory_t *)stream;

    if (size > mem->size - mem->cur_offset)
      size = mem->size - mem->cur_offset;

    memcpy(buf, mem->base + mem->cur_offset, size);
    mem->cur_offset+=size;

    return size;
}


uLong ZCALLBACK fwrite_mem_func (opaque, stream, buf, size)
   voidpf opaque;
   voidpf stream;
   const void* buf;
   uLong size;
{
    ourmemory_t *mem = (ourmemory_t *)stream;

    if (size > mem->size - mem->cur_offset)
      size = mem->size - mem->cur_offset;

    memcpy(mem->base + mem->cur_offset, buf, size);
    mem->cur_offset+=size;
    if (mem->cur_offset > mem->limit)
      mem->limit = mem->cur_offset;

    return size;
}

long ZCALLBACK ftell_mem_func (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
    ourmemory_t *mem = (ourmemory_t *)stream;

    return mem->cur_offset;
}

long ZCALLBACK fseek_mem_func (opaque, stream, offset, origin)
   voidpf opaque;
   voidpf stream;
   uLong offset;
   int origin;
{
    ourmemory_t *mem = (ourmemory_t *)stream;
    uLong new_pos;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        new_pos = mem->cur_offset + offset;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        new_pos = mem->limit + offset;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        new_pos = offset;
        break;
    default: return -1;
    }

    if (new_pos > mem->size)
      return 1; /* Failed to seek that far */

    if (new_pos > mem->limit)
      memset(mem->base + mem->limit, 0, new_pos - mem->limit);

    mem->cur_offset = new_pos;
    return 0;
}

int ZCALLBACK fclose_mem_func (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
    ourmemory_t *mem = (ourmemory_t *)stream;

    /* Note that once we've written to the buffer we don't tell anyone
       about it here. Probably the opaque handle could be used to inform
       some other component of how much data was written.
       This, and other aspects of writing through this interface, has
       not been tested.
     */

    free (mem);
    return 0;
}

int ZCALLBACK ferror_mem_func (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
    //ourmemory_t *mem = (ourmemory_t *)stream;
    /* We never return errors */
    return 0;
}

void fill_memory_filefunc (pzlib_filefunc_def)
  zlib_filefunc_def* pzlib_filefunc_def;
{
    pzlib_filefunc_def->zopen_file = fopen_mem_func;
    pzlib_filefunc_def->zread_file = fread_mem_func;
    pzlib_filefunc_def->zwrite_file = fwrite_mem_func;
    pzlib_filefunc_def->ztell_file = ftell_mem_func;
    pzlib_filefunc_def->zseek_file = fseek_mem_func;
    pzlib_filefunc_def->zclose_file = fclose_mem_func;
    pzlib_filefunc_def->zerror_file = ferror_mem_func;
    pzlib_filefunc_def->opaque = NULL;
}
