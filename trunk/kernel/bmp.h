/* Note: the magic number has been removed from the bmp_header structure
   since it causes alignment problems
     struct bmpfile_magic should be written/read first
   followed by the
     struct bmpfile_header
   [this avoids compiler-specific alignment pragmas etc.]
*/

#ifndef __BMP_FILE_H__
#define __BMP_FILE_H__
 
typedef struct  {
  unsigned char magic[2];
} bmpfile_magic;
 
typedef struct  {
	unsigned int filesz;
	unsigned short creator1;
	unsigned short creator2;
	unsigned int bmp_offset;
} bmpfile_header;

 typedef struct {
  unsigned int header_sz;
  unsigned int width;
  unsigned int height;
  unsigned short nplanes;
  unsigned short bitspp;
  unsigned int compress_type;
  unsigned int bmp_bytesz;
  unsigned int hres;
  unsigned int vres;
  unsigned int ncolors;
  unsigned int nimpcolors;
} bmp_dib_v3_header_t;


typedef enum {
  BI_RGB = 0,
  BI_RLE8,
  BI_RLE4,
  BI_BITFIELDS, //Also Huffman 1D compression for BITMAPCOREHEADER2
  BI_JPEG,      //Also RLE-24 compression for BITMAPCOREHEADER2
  BI_PNG,
} bmp_compression_method_t;

#endif