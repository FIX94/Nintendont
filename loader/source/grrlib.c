/*------------------------------------------------------------------------------
Copyright (c) 2010 The GRRLIB Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
------------------------------------------------------------------------------*/

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ogc/conf.h>
//#include <fat.h>
#include <wchar.h>
#include <ft2build.h>
#include <pngu.h>
#include <stdio.h>
#ifdef ENABLE_JPEG
#include <jpeglib.h>
#endif
#include <string.h>


#define __GRRLIB_CORE__
#include "grrlib.h"
#include "global.h"
#include "exi.h"
#include "ff_utf8.h"

#define DEFAULT_FIFO_SIZE (256 * 1024) /**< GX fifo buffer size. */

//#define ENABLE_JPEG
#define ENABLE_TTF

GRRLIB_drawSettings  GRRLIB_Settings;
static Mtx           GXmodelView2D;

static void  *gp_fifo = NULL;

static bool  is_setup = false;  // To control entry and exit
static bool  enable_output;

/**
 * Initialize GRRLIB. Call this once at the beginning your code.
 * @return A integer representating a code:
 *         -     0 : The operation completed successfully.
 *         -    -1 : Not enough memory is available to initialize GRRLIB.
 *         -    -2 : Failed to add the fat device driver to the devoptab.
 *         -    -3 : Failed to initialize the font engine.
 * @see GRRLIB_Exit
 */
int  GRRLIB_Init (void) {
	f32 yscale;
	u32 xfbHeight;
	Mtx44 perspective;
	s8 error_code = 0;

	// Ensure this function is only ever called once
	if (is_setup)  return 0;
	
	VIDEO_Init();

	if(CONF_GetProgressiveScan() > 0 && VIDEO_HaveComponentCable())
	{
		rmode = &TVNtsc480Prog;
		gprintf("PROG\n");
	}
	else
	{
		switch(CONF_GetVideo())
		{
			case CONF_VIDEO_PAL:
				if (CONF_GetEuRGB60() > 0)
				{
					gprintf("PAL60\n");
					rmode = &TVEurgb60Hz480Int;
				}
				else
				{
					gprintf("PAL50\n");
					rmode = &TVPal576IntDfScale;
				}
				break;
			case CONF_VIDEO_MPAL:
				gprintf("MPAL\n");
				rmode = &TVMpal480IntDf;
				break;
			case CONF_VIDEO_NTSC:
			default:
				gprintf("NTSC\n");
				rmode = &TVNtsc480Int;
				break;
		}
	}
	VIDEO_Configure(rmode);

	// Get some memory to use for a "double buffered" frame buffer
	if ( !(xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode))) )  return -1;
	if ( !(xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode))) )  return -1;

	VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);

	VIDEO_SetNextFramebuffer(xfb[fb]);  // Choose a frame buffer to start with

	VIDEO_Flush();                      // flush the frame to the TV
	VIDEO_WaitVSync();                  // Wait for the TV to finish updating
	// If the TV image is interlaced it takes two passes to display the image
	if (rmode->viTVMode & VI_NON_INTERLACE)  VIDEO_WaitVSync();

	// The FIFO is the buffer the CPU uses to send commands to the GPU
	if ( !(gp_fifo = memalign(32, DEFAULT_FIFO_SIZE)) )  return -1;
	memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

	// Clear the background to opaque black and clears the z-buffer
	GX_SetCopyClear((GXColor){ 0, 0, 0, 0 }, GX_MAX_Z24);

	if (rmode->aa)  GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);  // Set 16 bit RGB565
	else            GX_SetPixelFmt(GX_PF_RGB8_Z24  , GX_ZC_LINEAR);  // Set 24 bit Z24

	// Other GX setup
	yscale    = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	GX_SetDispCopyGamma(GX_GM_1_0);

	if(rmode->fbWidth <= 0){ printf("GRRLIB " GRRLIB_VER_STRING); }

	// Setup the vertex descriptor
	GX_ClearVtxDesc();      // clear all the vertex descriptors
	GX_InvVtxCache();       // Invalidate the vertex cache
	GX_InvalidateTexAll();  // Invalidate all textures

	// Tells the flipper to expect direct data
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST,   GX_F32, 0);
	// Colour 0 is 8bit RGBA format
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_TRUE);

	GX_SetNumChans(1);    // colour is the same as vertex colour
	GX_SetNumTexGens(1);  // One texture exists
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	guMtxIdentity(GXmodelView2D);
	guMtxTransApply(GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -100.0F);
	GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

	guOrtho(perspective, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 1000.0f);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetCullMode(GX_CULL_NONE);
	GRRLIB_ClipReset();

	// Default settings
	GRRLIB_Settings.antialias = true;
	GRRLIB_Settings.blend     = GRRLIB_BLEND_ALPHA;
	GRRLIB_Settings.lights    = 0;

	// Schedule cleanup for when program exits
	is_setup = true;
	atexit(GRRLIB_Exit);
	enable_output = false;
	// Initialise the filing system
	//if (!fatInitDefault())  error_code = -2;
	// Initialise TTF
#ifdef ENABLE_TTF
	if (GRRLIB_InitTTF())  error_code = -3;
#endif
    //VIDEO_SetBlack(false);  // Enable video output (let do this to render subr)
	return error_code;
}

/**
 * Call this before exiting your application.
 * Ensure this function is only ever called once
 * and only if the setup function has been called.
 */
void  GRRLIB_Exit (void)
	{
	if (!is_setup)
		return;
	else
		is_setup = false;

	// Allow write access to the full screen
	GX_SetClipMode( GX_CLIP_DISABLE );
	GX_SetScissor( 0, 0, rmode->fbWidth, rmode->efbHeight );

	// We empty both frame buffers on our way out
	// otherwise dead frames are sometimes seen when starting the next app
	GRRLIB_FillScreen( 0x000000FF );  GRRLIB_Render();
	GRRLIB_FillScreen( 0x000000FF );  GRRLIB_Render();

	// Shut down the GX engine
	GX_DrawDone();
	GX_AbortFrame();
	GX_Flush();

    if (gp_fifo != NULL) {  free(gp_fifo);               gp_fifo = NULL;  }

	// Free up memory allocated for frame buffers & FIFOs
	if (xfb[0]  != NULL) {  free(MEM_K1_TO_K0(xfb[0]));  xfb[0]  = NULL;  }
	if (xfb[1]  != NULL) {  free(MEM_K1_TO_K0(xfb[1]));  xfb[1]  = NULL;  }
	if (gp_fifo != NULL) {  free(gp_fifo);               gp_fifo = NULL;  }

	//VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

    // Done with TTF
#ifdef ENABLE_TTF
    GRRLIB_ExitTTF();
#endif
}

void  GRRLIB_ExitLight (void)
	{
	if (!is_setup)
		return;
    else
		is_setup = false;

    // Allow write access to the full screen
    GX_SetClipMode( GX_CLIP_DISABLE );
    GX_SetScissor( 0, 0, rmode->fbWidth, rmode->efbHeight );

    // Shut down the GX engine
    GX_DrawDone();
    GX_AbortFrame();
	GX_Flush();

    if (gp_fifo != NULL) {  free(gp_fifo);               gp_fifo = NULL;  }

    // Free up memory allocated for frame buffers & FIFOs
    if (xfb[0]  != NULL) {  free(MEM_K1_TO_K0(xfb[0]));  xfb[0]  = NULL;  }
    if (xfb[1]  != NULL) {  free(MEM_K1_TO_K0(xfb[1]));  xfb[1]  = NULL;  }

	// Done with TTF
#ifdef ENABLE_TTF
    GRRLIB_ExitTTF();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_ttf
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include FT_FREETYPE_H

static FT_Library ftLibrary;        /**< A handle to a FreeType library instance. */

// Static function prototypes
static void DrawBitmap(FT_Bitmap *bitmap, int offset, int top, const u8 cR, const u8 cG, const u8 cB);


/**
 * Initialize FreeType library.
 * @return int 0=OK; -1=Failed
 */
int GRRLIB_InitTTF () {
	if (FT_Init_FreeType(&ftLibrary)) {
		return -1;
	}
	return 0;
}

/**
 * Call this when your done with FreeType.
 */
void GRRLIB_ExitTTF (void) {
	FT_Done_FreeType(ftLibrary);
}

/**
 * Load a TTF from a buffer.
 * @param file_base Buffer with TTF data. You must not deallocate the memory before calling GRRLIB_FreeTTF.
 * @param file_size Size of the TTF buffer.
 * @return A handle to a given TTF font object.
 * @see GRRLIB_FreeTTF
 */
GRRLIB_ttfFont* GRRLIB_LoadTTF (const u8* file_base, s32 file_size) {
	FT_Face Face;
	GRRLIB_ttfFont* myFont = (GRRLIB_ttfFont*)malloc(sizeof(GRRLIB_ttfFont));
	FT_New_Memory_Face(ftLibrary, file_base, file_size, 0, &Face);
	myFont->kerning = FT_HAS_KERNING(Face);
/*
	if (FT_Set_Pixel_Sizes(Face, 0, fontSize)) {
		FT_Set_Pixel_Sizes(Face, 0, 12);
	}
*/
	myFont->face = Face;
	return myFont;
}

/**
 * Free memory allocated by TTF fonts.
 * @param myFont A TTF.
 */
void  GRRLIB_FreeTTF (GRRLIB_ttfFont *myFont) {
	if(myFont) {
		FT_Done_Face(myFont->face);
		free(myFont);
		myFont = NULL;
	}
}

/**
 * Print function for TTF font.
 * @param x Specifies the x-coordinate of the upper-left corner of the text.
 * @param y Specifies the y-coordinate of the upper-left corner of the text.
 * @param myFont A TTF.
 * @param string Text to draw.
 * @param fontSize Size of the font.
 * @param color Text color in RGBA format.
 */
void GRRLIB_PrintfTTF(int x, int y, GRRLIB_ttfFont *myFont, const char *string, unsigned int fontSize, const u32 color) {
	if(myFont == NULL || string == NULL)
		return;

	size_t length = strlen(string) + 1;
	wchar_t *utf32 = (wchar_t*)malloc(length * sizeof(wchar_t));
	if(utf32) {
		length = mbstowcs(utf32, string, length);
		if(length > 0) {
			utf32[length] = L'\0';
			GRRLIB_PrintfTTFW(x, y, myFont, utf32, fontSize, color);
		}
		free(utf32);
	}
}

/**
 * Print function for TTF font.
 * @author wplaat and DrTwox
 * @param x Specifies the x-coordinate of the upper-left corner of the text.
 * @param y Specifies the y-coordinate of the upper-left corner of the text.
 * @param myFont A TTF.
 * @param utf32 Text to draw.
 * @param fontSize Size of the font.
 * @param color Text color in RGBA format.
 */
void GRRLIB_PrintfTTFW(int x, int y, GRRLIB_ttfFont *myFont, const wchar_t *utf32, unsigned int fontSize, const u32 color) {
	if(myFont == NULL || utf32 == NULL)
		return;

	FT_Face Face = (FT_Face)myFont->face;
	int penX = 0;
	int penY = fontSize;
	FT_GlyphSlot slot = Face->glyph;
	FT_UInt glyphIndex = 0;
	FT_UInt previousGlyph = 0;
	u8 cR = R(color), cG = G(color), cB = B(color);

	if (FT_Set_Pixel_Sizes(Face, 0, fontSize)) {
		FT_Set_Pixel_Sizes(Face, 0, 12);
	}

	/* Loop over each character, until the
	 * end of the string is reached, or until the pixel width is too wide */
	while(*utf32) {
		glyphIndex = FT_Get_Char_Index(myFont->face, *utf32++);

		if (myFont->kerning && previousGlyph && glyphIndex) {
			FT_Vector delta;
			FT_Get_Kerning(myFont->face, previousGlyph, glyphIndex, FT_KERNING_DEFAULT, &delta);
			penX += delta.x >> 6;
		}
		if (FT_Load_Glyph(myFont->face, glyphIndex, FT_LOAD_RENDER)) {
			continue;
		}

		DrawBitmap(&slot->bitmap,
				   penX + slot->bitmap_left + x,
				   penY - slot->bitmap_top + y,
				   cR, cG, cB);
		penX += slot->advance.x >> 6;
		previousGlyph = glyphIndex;
	}
}

/**
 * Draw a character on the screen.
 * @param bitmap Bitmap to draw.
 * @param offset x-coordinate offset.
 * @param top y-coordinate.
 * @param cR Red component of the colour.
 * @param cG Green component of the colour.
 * @param cB Blue component of the colour.
 */
static void DrawBitmap(FT_Bitmap *bitmap, int offset, int top, const u8 cR, const u8 cG, const u8 cB) {
	FT_Int i, j, p, q;
	FT_Int x_max = offset + bitmap->width;
	FT_Int y_max = top + bitmap->rows;

	for ( i = offset, p = 0; i < x_max; i++, p++ ) {
		for ( j = top, q = 0; j < y_max; j++, q++ ) {
			GX_Begin(GX_POINTS, GX_VTXFMT0, 1);
				GX_Position3f32(i, j, 0);
				GX_Color4u8(cR, cG, cB,
							bitmap->buffer[ q * bitmap->width + p ]);
			GX_End();
		}
	}
}

/**
 * Get the width of a text in pixel.
 * @param myFont A TTF.
 * @param string The text to check.
 * @param fontSize The size of the font.
 * @return The width of a text in pixel.
 */
unsigned int GRRLIB_WidthTTF(GRRLIB_ttfFont *myFont, const char *string, unsigned int fontSize) {
	if(myFont == NULL || string == NULL) {
		return 0;
	}
	unsigned int penX = 0;
	int length = strlen(string) + 1;
	wchar_t *utf32 = (wchar_t*)malloc(length * sizeof(wchar_t)+1);
	if (!utf32) return 0;
		length = mbstowcs(utf32, string, length);
	if (length > 0)
	{
		utf32[length] = L'\0';
		penX = GRRLIB_WidthTTFW(myFont, utf32, fontSize);
	}
	free(utf32);
	return penX;

/*	unsigned int penX;
	size_t length = strlen(string) + 1;
	wchar_t *utf32 = (wchar_t*)malloc(length * sizeof(wchar_t));
	length = mbstowcs(utf32, string, length);
	utf32[length] = L'\0';

	penX = GRRLIB_WidthTTFW(myFont, utf32, fontSize);

	free(utf32);

	return penX;*/
}

/**
 * Get the width of a text in pixel.
 * @param myFont A TTF.
 * @param utf32 The text to check.
 * @param fontSize The size of the font.
 * @return The width of a text in pixel.
 */
unsigned int GRRLIB_WidthTTFW(GRRLIB_ttfFont *myFont, const wchar_t *utf32, unsigned int fontSize) {
	if(myFont == NULL || utf32 == NULL) {
		return 0;
	}

	FT_Face Face = (FT_Face)myFont->face;
	unsigned int penX = 0;
	FT_UInt glyphIndex;
	FT_UInt previousGlyph = 0;

	if(FT_Set_Pixel_Sizes(myFont->face, 0, fontSize)) {
		 FT_Set_Pixel_Sizes(myFont->face, 0, 12);
	}

	while(*utf32) {
		glyphIndex = FT_Get_Char_Index(myFont->face, *utf32++);

		if(myFont->kerning && previousGlyph && glyphIndex) {
			FT_Vector delta;
			FT_Get_Kerning(Face, previousGlyph, glyphIndex, FT_KERNING_DEFAULT, &delta);
			penX += delta.x >> 6;
		}
		if(FT_Load_Glyph(Face, glyphIndex, FT_LOAD_RENDER)) {
			continue;
		}

		penX += Face->glyph->advance.x >> 6;
		previousGlyph = glyphIndex;
	}

	return penX;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_texEdit
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * This structure contains information about the type, size, and layout of a file that containing a device-independent bitmap (DIB).
 */
typedef  struct tagBITMAPFILEHEADER {
	u16 bfType;             /**< Specifies the file type. It must be set to the signature word BM (0x4D42) to indicate bitmap. */
	u32 bfSize;             /**< Specifies the size, in bytes, of the bitmap file. */
	u16 bfReserved1;        /**< Reserved; set to zero. */
	u16 bfReserved2;        /**< Reserved; set to zero. */
	u32 bfOffBits;          /**< Specifies the offset, in bytes, from the BITMAPFILEHEADER structure to the bitmap bits. */
} BITMAPFILEHEADER;
/**
 * This structure contains information about the dimensions and color format of a device-independent bitmap (DIB).
 */
typedef  struct tagBITMAPINFOHEADER {
	u32 biSize;             /**< Specifies the size of the structure, in bytes. */
	u32 biWidth;            /**< Specifies the width of the bitmap, in pixels. */
	u32 biHeight;           /**< Specifies the height of the bitmap, in pixels. */
	u16 biPlanes;           /**< Specifies the number of planes for the target device. */
	u16 biBitCount;         /**< Specifies the number of bits per pixel. */
	u32 biCompression;      /**< Specifies the type of compression for a compressed bottom-up bitmap.*/
	u32 biSizeImage;        /**< Specifies the size, in bytes, of the image. */
	u32 biXPelsPerMeter;    /**< Specifies the horizontal resolution, in pixels per meter, of the target device for the bitmap. */
	u32 biYPelsPerMeter;    /**< Specifies the vertical resolution, in pixels per meter, of the target device for the bitmap. */
	u32 biClrUsed;          /**< Specifies the number of color indexes in the color table that are actually used by the bitmap. */
	u32 biClrImportant;     /**< Specifies the number of color indexes required for displaying the bitmap. */
} BITMAPINFOHEADER;
/**
 * The RGBQUAD structure describes a color consisting of relative intensities of
 * red, green, and blue. The bmiColors member of the BITMAPINFO structure
 * consists of an array of RGBQUAD structures.
 */
typedef  struct tagRGBQUAD {
	u8 rgbBlue;             /**< Specifies the intensity of blue in the color. */
	u8 rgbGreen;            /**< Specifies the intensity of green in the color. */
	u8 rgbRed;              /**< Specifies the intensity of red in the color. */
	u8 rgbReserved;         /**< Not used; must be set to zero. */
} RGBQUAD;

/**
 * Convert a raw BMP (RGB, no alpha) to 4x4RGBA.
 * @author DragonMinded
 * @param src Source.
 * @param dst Destination.
 * @param width Width.
 * @param height Height.
*/
#ifdef ENABLE_JPEG
static
void  RawTo4x4RGBA (const u8 *src, void *dst,
					const uint width, const uint height) {
	uint  block;
	uint  i;
	u8    c;
	u8    argb;

	u8    *p = (u8*)dst;

	for (block = 0; block < height; block += 4) {
		for (i = 0; i < width; i += 4) {
			// Alpha and Red
			for (c = 0; c < 4; ++c) {
				for (argb = 0; argb < 4; ++argb) {
					// Alpha pixels
					*p++ = 255;
					// Red pixels
					*p++ = src[((i + argb) + ((block + c) * width)) * 3];
				}
			}

			// Green and Blue
			for (c = 0; c < 4; ++c) {
				for (argb = 0; argb < 4; ++argb) {
					// Green pixels
					*p++ = src[(((i + argb) + ((block + c) * width)) * 3) + 1];
					// Blue pixels
					*p++ = src[(((i + argb) + ((block + c) * width)) * 3) + 2];
				}
			}
		}
	}
}
#endif

/**
 * Load a texture from a buffer.
 * @param my_img The JPEG, PNG or Bitmap buffer to load.
 * @return A GRRLIB_texImg structure filled with image information.
 */
GRRLIB_texImg*  GRRLIB_LoadTexture (const u8 *my_img) {
	if (my_img[0]==0xFF && my_img[1]==0xD8 && my_img[2]==0xFF)
		return (GRRLIB_LoadTextureJPG(my_img));
	else if (my_img[0]=='B' && my_img[1]=='M')
		return (GRRLIB_LoadTextureBMP(my_img));
	else
		return (GRRLIB_LoadTexturePNG(my_img));
}

/**
 * Load a texture from a buffer.
 * @param my_png the PNG buffer to load.
 * @return A GRRLIB_texImg structure filled with image information.
 *         If image size is not correct, the texture will be completely transparent.
 */
GRRLIB_texImg*  GRRLIB_LoadTexturePNG (const u8 *my_png) {
	int width = 0, height = 0;
	PNGUPROP imgProp;
	IMGCTX ctx;
	GRRLIB_texImg *my_texture = calloc(1, sizeof(GRRLIB_texImg));

	if(my_texture != NULL) {
		ctx = PNGU_SelectImageFromBuffer(my_png);
		PNGU_GetImageProperties(ctx, &imgProp);
		my_texture->data = PNGU_DecodeTo4x4RGBA8(ctx, imgProp.imgWidth, imgProp.imgHeight, &width, &height, NULL);
		if(my_texture->data != NULL) {
			my_texture->w = width;
			my_texture->h = height;
			GRRLIB_SetHandle( my_texture, 0, 0 );
			if(imgProp.imgWidth != width || imgProp.imgHeight != height) {
				// PNGU has resized the texture
				memset(my_texture->data, 0, (my_texture->h * my_texture->w) << 2);
			}
			GRRLIB_FlushTex( my_texture );
		}
		PNGU_ReleaseImageContext(ctx);
	}
	return my_texture;
}

/**
 * Create an array of palette.
 * @param my_bmp Bitmap buffer to parse.
 * @param Size The number of palette to add.
 * @return An array of palette. Memory must be deleted.
 */
static RGBQUAD*  GRRLIB_CreatePalette (const u8 *my_bmp, u32 Size) {
	u32 n, i = 0;
	RGBQUAD *Palette = calloc(Size, sizeof(RGBQUAD));
	for(n=0; n<Size; n++) {
		Palette[n].rgbBlue = my_bmp[i];
		Palette[n].rgbGreen = my_bmp[i+1];
		Palette[n].rgbRed = my_bmp[i+2];
		Palette[n].rgbReserved = 0;
		i += 4;
	}
	return Palette;
}

/**
 * Load a texture from a buffer.
 * It only works for the MS-Windows standard format uncompressed (1-bit, 4-bit, 8-bit, 24-bit and 32-bit).
 * @param my_bmp the Bitmap buffer to load.
 * @return A GRRLIB_texImg structure filled with image information.
 */
GRRLIB_texImg*  GRRLIB_LoadTextureBMP (const u8 *my_bmp) {
	BITMAPFILEHEADER MyBitmapFileHeader;
	BITMAPINFOHEADER MyBitmapHeader;
	RGBQUAD *Palette;
	u16 pal_ref;
	u32 BufferSize;
	s32 y, x, i;
	GRRLIB_texImg *my_texture = calloc(1, sizeof(GRRLIB_texImg));

	if(my_texture != NULL) {
		// Fill file header structure
		MyBitmapFileHeader.bfType      = (my_bmp[0]  | my_bmp[1]<<8);
		MyBitmapFileHeader.bfSize      = (my_bmp[2]  | my_bmp[3]<<8  | my_bmp[4]<<16 | my_bmp[5]<<24);
		MyBitmapFileHeader.bfReserved1 = (my_bmp[6]  | my_bmp[7]<<8);
		MyBitmapFileHeader.bfReserved2 = (my_bmp[8]  | my_bmp[9]<<8);
		MyBitmapFileHeader.bfOffBits   = (my_bmp[10] | my_bmp[11]<<8 | my_bmp[12]<<16 | my_bmp[13]<<24);
		// Fill the bitmap structure
		MyBitmapHeader.biSize          = (my_bmp[14] | my_bmp[15]<<8 | my_bmp[16]<<16 | my_bmp[17]<<24);
		MyBitmapHeader.biWidth         = (my_bmp[18] | my_bmp[19]<<8 | my_bmp[20]<<16 | my_bmp[21]<<24);
		MyBitmapHeader.biHeight        = (my_bmp[22] | my_bmp[23]<<8 | my_bmp[24]<<16 | my_bmp[25]<<24);
		MyBitmapHeader.biPlanes        = (my_bmp[26] | my_bmp[27]<<8);
		MyBitmapHeader.biBitCount      = (my_bmp[28] | my_bmp[29]<<8);
		MyBitmapHeader.biCompression   = (my_bmp[30] | my_bmp[31]<<8 | my_bmp[32]<<16 | my_bmp[33]<<24);
		MyBitmapHeader.biSizeImage     = (my_bmp[34] | my_bmp[35]<<8 | my_bmp[36]<<16 | my_bmp[37]<<24);
		MyBitmapHeader.biXPelsPerMeter = (my_bmp[38] | my_bmp[39]<<8 | my_bmp[40]<<16 | my_bmp[41]<<24);
		MyBitmapHeader.biYPelsPerMeter = (my_bmp[42] | my_bmp[43]<<8 | my_bmp[44]<<16 | my_bmp[45]<<24);
		MyBitmapHeader.biClrUsed       = (my_bmp[46] | my_bmp[47]<<8 | my_bmp[48]<<16 | my_bmp[49]<<24);
		MyBitmapHeader.biClrImportant  = (my_bmp[50] | my_bmp[51]<<8 | my_bmp[52]<<16 | my_bmp[53]<<24);

		my_texture->data = memalign(32, MyBitmapHeader.biWidth * MyBitmapHeader.biHeight * 4);
		if(my_texture->data != NULL && MyBitmapFileHeader.bfType == 0x4D42) {
			my_texture->w = MyBitmapHeader.biWidth;
			my_texture->h = MyBitmapHeader.biHeight;
			switch(MyBitmapHeader.biBitCount) {
				case 32:    // RGBA images
					i = 54;
					for(y=MyBitmapHeader.biHeight-1; y>=0; y--) {
						for(x=0; x<MyBitmapHeader.biWidth; x++) {
							GRRLIB_SetPixelTotexImg(x, y, my_texture,
								RGBA(my_bmp[i+2], my_bmp[i+1], my_bmp[i], my_bmp[i+3]));
							i += 4;
						}
					}
					break;
				case 24:    // truecolor images
					BufferSize = (MyBitmapHeader.biWidth % 4);
					i = 54;
					for(y=MyBitmapHeader.biHeight-1; y>=0; y--) {
						for(x=0; x<MyBitmapHeader.biWidth; x++) {
							GRRLIB_SetPixelTotexImg(x, y, my_texture,
								RGBA(my_bmp[i+2], my_bmp[i+1], my_bmp[i], 0xFF));
							i += 3;
						}
						i += BufferSize;   // Padding
					}
					break;
				case 8:     // 256 color images
					BufferSize = (int) MyBitmapHeader.biWidth;
					while(BufferSize % 4) {
						BufferSize++;
					}
					BufferSize -= MyBitmapHeader.biWidth;
					Palette = GRRLIB_CreatePalette(&my_bmp[54], 256);
					i = 1078; // 54 + (MyBitmapHeader.biBitCount * 4)
					for(y=MyBitmapHeader.biHeight-1; y>=0; y--) {
						for(x=0; x<MyBitmapHeader.biWidth; x++) {
							GRRLIB_SetPixelTotexImg(x, y, my_texture,
								RGBA(Palette[my_bmp[i]].rgbRed,
									 Palette[my_bmp[i]].rgbGreen,
									 Palette[my_bmp[i]].rgbBlue,
									 0xFF));
							i++;
						}
						i += BufferSize;   // Padding
					}
					free(Palette);
					break;
				case 4:     // 16 color images
					BufferSize = (int)((MyBitmapHeader.biWidth*4) / 8.0);
					while(8*BufferSize < MyBitmapHeader.biWidth*4) {
						BufferSize++;
					}
					while(BufferSize % 4) {
						BufferSize++;
					}
					Palette = GRRLIB_CreatePalette(&my_bmp[54], 16);
					i = 118; // 54 + (MyBitmapHeader.biBitCount * 4)
					for(y=MyBitmapHeader.biHeight-1; y>=0; y--) {
						for(x=0; x<MyBitmapHeader.biWidth; x++) {
							pal_ref = (my_bmp[i + (x / 2)] >> ((x % 2) ? 0 : 4)) & 0x0F;
							GRRLIB_SetPixelTotexImg(x, y, my_texture,
								RGBA(Palette[pal_ref].rgbRed,
									 Palette[pal_ref].rgbGreen,
									 Palette[pal_ref].rgbBlue,
									 0xFF));
						}
						i += BufferSize;   // Padding
					}
					free(Palette);
					break;
				case 1:     // black & white images
					BufferSize = (int)(MyBitmapHeader.biWidth / 8.0);
					while(8*BufferSize < MyBitmapHeader.biWidth) {
						BufferSize++;
					}
					while(BufferSize % 4) {
						BufferSize++;
					}
					Palette = GRRLIB_CreatePalette(&my_bmp[54], 2);
					i = 62; // 54 + (MyBitmapHeader.biBitCount * 4)
					for(y=MyBitmapHeader.biHeight-1; y>=0; y--) {
						for(x=0; x<MyBitmapHeader.biWidth; x++) {
							pal_ref = (my_bmp[i + (x / 8)] >> (-x%8+7)) & 0x01;
							GRRLIB_SetPixelTotexImg(x, y, my_texture,
								RGBA(Palette[pal_ref].rgbRed,
									 Palette[pal_ref].rgbGreen,
									 Palette[pal_ref].rgbBlue,
									 0xFF));
						}
						i += BufferSize;   // Padding
					}
					free(Palette);
					break;
				default:
					GRRLIB_ClearTex(my_texture);
			}
			GRRLIB_SetHandle( my_texture, 0, 0 );
			GRRLIB_FlushTex( my_texture );
		}
	}
	return my_texture;
}

/**
 * Load a texture from a buffer.
 * Take care to have the JPG finish with 0xFF 0xD9!
 * @param my_jpg The JPEG buffer to load.
 * @return A GRRLIB_texImg structure filled with image information.
 */
GRRLIB_texImg*  GRRLIB_LoadTextureJPG (const u8 *my_jpg) {
	int n = 0;

	if ((my_jpg[0]==0xFF) && (my_jpg[1]==0xD8) && (my_jpg[2]==0xFF)) {
		while(true) {
			if ((my_jpg[n]==0xFF) && (my_jpg[n+1]==0xD9))
				break;
			n++;
		}
		n+=2;
	}

#ifdef ENABLE_JPEG
	return GRRLIB_LoadTextureJPGEx(my_jpg, n);
#else
    return NULL;
#endif
}

/**
 * Load a texture from a buffer.
 * @author DrTwox
 * @param my_jpg The JPEG buffer to load.
 * @param my_size Size of the JPEG buffer to load.
 * @return A GRRLIB_texImg structure filled with image information.
 */
 #ifdef ENABLE_JPEG
GRRLIB_texImg*  GRRLIB_LoadTextureJPGEx (const u8 *my_jpg, const int my_size) {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	GRRLIB_texImg *my_texture = calloc(1, sizeof(GRRLIB_texImg));
	unsigned int i;

	if(my_texture == NULL)
		return NULL;

	jpeg_create_decompress(&cinfo);
	cinfo.err = jpeg_std_error(&jerr);
	cinfo.progress = NULL;
	jpeg_mem_src(&cinfo, (unsigned char *)my_jpg, my_size);
	jpeg_read_header(&cinfo, TRUE);
	if(cinfo.jpeg_color_space == JCS_GRAYSCALE) {
		cinfo.out_color_space = JCS_RGB;
	}
	jpeg_start_decompress(&cinfo);
	unsigned char *tempBuffer = malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);
	JSAMPROW row_pointer[1];
	row_pointer[0] = malloc(cinfo.output_width * cinfo.output_components);
	size_t location = 0;
	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, row_pointer, 1);
		for (i = 0; i < cinfo.image_width * cinfo.output_components; i++) {
			/* Put the decoded scanline into the tempBuffer */
			tempBuffer[ location++ ] = row_pointer[0][i];
		}
	}

	/* Create a buffer to hold the final texture */
	my_texture->data = memalign(32, cinfo.output_width * cinfo.output_height * 4);
	RawTo4x4RGBA(tempBuffer, my_texture->data, cinfo.output_width, cinfo.output_height);

	/* Done - Do cleanup and release allocated memory */
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	free(row_pointer[0]);
	free(tempBuffer);

	my_texture->w = cinfo.output_width;
	my_texture->h = cinfo.output_height;
	GRRLIB_SetHandle( my_texture, 0, 0 );
	GRRLIB_FlushTex( my_texture );
	return my_texture;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_snapshot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Make a snapshot of the screen in a texture WITHOUT ALPHA LAYER.
 * @param posx top left corner of the grabbed part.
 * @param posy top left corner of the grabbed part.
 * @param tex A pointer to a texture representing the screen or NULL if an error occurs.
 * @param clear When this flag is set to true, the screen is cleared after copy.
 */
void  GRRLIB_Screen2Texture (int posx, int posy, GRRLIB_texImg *tex, bool clear) {
	if(tex->data != NULL) {
		GX_SetTexCopySrc(posx, posy, tex->w, tex->h);
		GX_SetTexCopyDst(tex->w, tex->h, GX_TF_RGBA8, GX_FALSE);
		GX_CopyTex(tex->data, GX_FALSE);
		GX_PixModeSync();
		GRRLIB_FlushTex(tex);
		if(clear)
			GX_CopyDisp(xfb[!fb], GX_TRUE);
	}
}

/**
 * Start GX compositing process.
 * @see GRRLIB_CompoEnd
 */
void GRRLIB_CompoStart (void) {
	GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
	GX_PokeAlphaRead(GX_READ_NONE);
}

/**
 * End GX compositing process (Make a snapshot of the screen in a texture WITH ALPHA LAYER).
 * EFB is cleared after this function.
 * @see GRRLIB_CompoStart
 * @param posx top left corner of the grabbed part.
 * @param posy top left corner of the grabbed part.
 * @param tex A pointer to a texture representing the screen or NULL if an error occurs.
 */
void GRRLIB_CompoEnd(int posx, int posy, GRRLIB_texImg *tex) {
	GRRLIB_Screen2Texture(posx, posy, tex, GX_TRUE);

	if (rmode->aa)  GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else            GX_SetPixelFmt(GX_PF_RGB8_Z24  , GX_ZC_LINEAR);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_render
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern  GRRLIB_drawSettings  GRRLIB_Settings;
extern  Mtx                  GXmodelView2D;

static  guVector  axis = {0, 0, 1};

/**
 * Draw a texture.
 * @param xpos Specifies the x-coordinate of the upper-left corner.
 * @param ypos Specifies the y-coordinate of the upper-left corner.
 * @param tex The texture to draw.
 * @param degrees Angle of rotation.
 * @param scaleX Specifies the x-coordinate scale. -1 could be used for flipping the texture horizontally.
 * @param scaleY Specifies the y-coordinate scale. -1 could be used for flipping the texture vertically.
 * @param color Color in RGBA format.
 */
void  GRRLIB_DrawImg (const f32 xpos, const f32 ypos, const GRRLIB_texImg *tex, const f32 degrees, const f32 scaleX, const f32 scaleY, const u32 color) {
	GXTexObj  texObj;
	u16       width, height;
	Mtx       m, m1, m2, mv;

	if (tex == NULL || tex->data == NULL)  return;

	GX_InitTexObj(&texObj, tex->data, tex->w, tex->h,
				  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);

	if (GRRLIB_Settings.antialias == false) {
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR,
						 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
		GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	}
	else {
		GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	}

	GX_LoadTexObj(&texObj,      GX_TEXMAP0);
	GX_SetTevOp  (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_DIRECT);

	guMtxIdentity  (m1);
	guMtxScaleApply(m1, m1, scaleX, scaleY, 1.0);
	guMtxRotAxisDeg(m2, &axis, degrees);
	guMtxConcat    (m2, m1, m);

	width  = tex->w * 0.5;
	height = tex->h * 0.5;

	guMtxTransApply(m, m,
		xpos +width  +tex->handlex
			-tex->offsetx +( scaleX *(-tex->handley *sin(-DegToRad(degrees))
									  -tex->handlex *cos(-DegToRad(degrees))) ),
		ypos +height +tex->handley
			-tex->offsety +( scaleY *(-tex->handley *cos(-DegToRad(degrees))
									  +tex->handlex *sin(-DegToRad(degrees))) ),
		0);
	guMtxConcat(GXmodelView2D, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(-width, -height, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(0, 0);

		GX_Position3f32(width, -height, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(1, 0);

		GX_Position3f32(width, height, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(1, 1);

		GX_Position3f32(-width, height, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(0, 1);
	GX_End();
	GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp  (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_NONE);
}

/**
 * Draw a textured quad.
 * @param pos Vector array of the 4 points.
 * @param tex The texture to draw.
 * @param color Color in RGBA format.
 */
void  GRRLIB_DrawImgQuad (const guVector pos[4], GRRLIB_texImg *tex, const u32 color) {
	GXTexObj  texObj;
	Mtx       m, m1, m2, mv;

	if (tex == NULL || tex->data == NULL)  return;

	GX_InitTexObj(&texObj, tex->data, tex->w, tex->h,
				  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);

	if (GRRLIB_Settings.antialias == false) {
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR,
						 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
		GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	}
	else {
		GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	}

	GX_LoadTexObj(&texObj,      GX_TEXMAP0);
	GX_SetTevOp  (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_DIRECT);

	guMtxIdentity  (m1);
	guMtxScaleApply(m1, m1, 1, 1, 1.0);
	guMtxRotAxisDeg(m2, &axis, 0);
	guMtxConcat    (m2, m1, m);
	guMtxConcat    (GXmodelView2D, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(pos[0].x, pos[0].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(0, 0);

		GX_Position3f32(pos[1].x, pos[1].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(1, 0);

		GX_Position3f32(pos[2].x, pos[2].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(1, 1);

		GX_Position3f32(pos[3].x, pos[3].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(0, 1);
	GX_End();
	GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp  (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_NONE);
}

/**
 * Draw a tile.
 * @param xpos Specifies the x-coordinate of the upper-left corner.
 * @param ypos Specifies the y-coordinate of the upper-left corner.
 * @param tex The texture containing the tile to draw.
 * @param degrees Angle of rotation.
 * @param scaleX Specifies the x-coordinate scale. -1 could be used for flipping the texture horizontally.
 * @param scaleY Specifies the y-coordinate scale. -1 could be used for flipping the texture vertically.
 * @param color Color in RGBA format.
 * @param frame Specifies the frame to draw.
 */
void  GRRLIB_DrawTile (const f32 xpos, const f32 ypos, const GRRLIB_texImg *tex, const f32 degrees, const f32 scaleX, const f32 scaleY, const u32 color, const int frame) {
	GXTexObj  texObj;
	f32       width, height;
	Mtx       m, m1, m2, mv;
	f32       s1, s2, t1, t2;

	if (tex == NULL || tex->data == NULL)  return;

	// The 0.001f/x is the frame correction formula by spiffen
	s1 = (frame % tex->nbtilew) * tex->ofnormaltexx;
	s2 = s1 + tex->ofnormaltexx;
	t1 = (int)(frame/tex->nbtilew) * tex->ofnormaltexy;
	t2 = t1 + tex->ofnormaltexy;

	GX_InitTexObj(&texObj, tex->data,
				  tex->tilew * tex->nbtilew, tex->tileh * tex->nbtileh,
				  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);

	if (GRRLIB_Settings.antialias == false) {
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR,
						 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
		GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	}
	else {
		GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	}

	GX_LoadTexObj(&texObj,      GX_TEXMAP0);
	GX_SetTevOp  (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_DIRECT);

	width  = tex->tilew * 0.5f;
	height = tex->tileh * 0.5f;

	guMtxIdentity  (m1);
	guMtxScaleApply(m1, m1, scaleX, scaleY, 1.0f);
	guMtxRotAxisDeg(m2, &axis, degrees);
	guMtxConcat    (m2, m1, m);

	guMtxTransApply(m, m,
		xpos +width  +tex->handlex
			-tex->offsetx +( scaleX *(-tex->handley *sin(-DegToRad(degrees))
									  -tex->handlex *cos(-DegToRad(degrees))) ),
		ypos +height +tex->handley
			-tex->offsety +( scaleY *(-tex->handley *cos(-DegToRad(degrees))
									  +tex->handlex *sin(-DegToRad(degrees))) ),
		0);

	guMtxConcat(GXmodelView2D, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(-width, -height, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s1, t1);

		GX_Position3f32(width, -height,  0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s2, t1);

		GX_Position3f32(width, height,  0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s2, t2);

		GX_Position3f32(-width, height,  0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s1, t2);
	GX_End();
	GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp  (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_NONE);
}

/**
 * Draw a part of a texture.
 * @param xpos Specifies the x-coordinate of the upper-left corner.
 * @param ypos Specifies the y-coordinate of the upper-left corner.
 * @param partx Specifies the x-coordinate of the upper-left corner in the texture.
 * @param party Specifies the y-coordinate of the upper-left corner in the texture.
 * @param partw Specifies the width in the texture.
 * @param parth Specifies the height in the texture.
 * @param tex The texture containing the tile to draw.
 * @param degrees Angle of rotation.
 * @param scaleX Specifies the x-coordinate scale. -1 could be used for flipping the texture horizontally.
 * @param scaleY Specifies the y-coordinate scale. -1 could be used for flipping the texture vertically.
 * @param color Color in RGBA format.
 */
void  GRRLIB_DrawPart (const f32 xpos, const f32 ypos, const f32 partx, const f32 party, const f32 partw, const f32 parth, const GRRLIB_texImg *tex, const f32 degrees, const f32 scaleX, const f32 scaleY, const u32 color) {
	GXTexObj  texObj;
	f32       width, height;
	Mtx       m, m1, m2, mv;
	f32       s1, s2, t1, t2;

	if (tex == NULL || tex->data == NULL)  return;

	// The 0.001f/x is the frame correction formula by spiffen
	s1 = (partx /tex->w) +(0.001f /tex->w);
	s2 = ((partx + partw)/tex->w) -(0.001f /tex->w);
	t1 = (party /tex->h) +(0.001f /tex->h);
	t2 = ((party + parth)/tex->h) -(0.001f /tex->h);

	GX_InitTexObj(&texObj, tex->data,
				  tex->w, tex->h,
				  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);

	if (GRRLIB_Settings.antialias == false) {
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR,
						 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
		GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	}
	else {
		GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	}

	GX_LoadTexObj(&texObj,      GX_TEXMAP0);
	GX_SetTevOp  (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_DIRECT);

	width  = partw * 0.5f;
	height = parth * 0.5f;

	guMtxIdentity  (m1);
	guMtxScaleApply(m1, m1, scaleX, scaleY, 1.0f);
	guMtxRotAxisDeg(m2, &axis, degrees);
	guMtxConcat    (m2, m1, m);

	guMtxTransApply(m, m,
		xpos +width  +tex->handlex
			-tex->offsetx +( scaleX *(-tex->handley *sin(-DegToRad(degrees))
									  -tex->handlex *cos(-DegToRad(degrees))) ),
		ypos +height +tex->handley
			-tex->offsety +( scaleY *(-tex->handley *cos(-DegToRad(degrees))
									  +tex->handlex *sin(-DegToRad(degrees))) ),
		0);

	guMtxConcat(GXmodelView2D, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(-width, -height, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s1, t1);

		GX_Position3f32(width, -height,  0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s2, t1);

		GX_Position3f32(width, height,  0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s2, t2);

		GX_Position3f32(-width, height,  0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s1, t2);
	GX_End();
	GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp  (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_NONE);
}

/**
 * Draw a tile in a quad.
 * @param pos Vector array of the 4 points.
 * @param tex The texture to draw.
 * @param color Color in RGBA format.
 * @param frame Specifies the frame to draw.
 */
void  GRRLIB_DrawTileQuad (const guVector pos[4], GRRLIB_texImg *tex, const u32 color, const int frame) {
	GXTexObj  texObj;
	Mtx       m, m1, m2, mv;
	f32       s1, s2, t1, t2;

	if (tex == NULL || tex->data == NULL)  return;

	// The 0.001f/x is the frame correction formula by spiffen
	s1 = ((     (frame %tex->nbtilew)   ) /(f32)tex->nbtilew) +(0.001f /tex->w);
	s2 = ((     (frame %tex->nbtilew) +1) /(f32)tex->nbtilew) -(0.001f /tex->w);
	t1 = (((int)(frame /tex->nbtilew)   ) /(f32)tex->nbtileh) +(0.001f /tex->h);
	t2 = (((int)(frame /tex->nbtilew) +1) /(f32)tex->nbtileh) -(0.001f /tex->h);

	GX_InitTexObj(&texObj, tex->data,
				  tex->tilew * tex->nbtilew, tex->tileh * tex->nbtileh,
				  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);

	if (GRRLIB_Settings.antialias == false) {
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR,
						 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
		GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	}
	else {
		GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	}

	GX_LoadTexObj(&texObj,      GX_TEXMAP0);
	GX_SetTevOp  (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_DIRECT);

	guMtxIdentity  (m1);
	guMtxScaleApply(m1, m1, 1, 1, 1.0f);
	guMtxRotAxisDeg(m2, &axis, 0);
	guMtxConcat    (m2, m1, m);
	guMtxConcat    (GXmodelView2D, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(pos[0].x, pos[0].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s1, t1);

		GX_Position3f32(pos[1].x, pos[1].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s2, t1);

		GX_Position3f32(pos[2].x, pos[2].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s2, t2);

		GX_Position3f32(pos[3].x, pos[3].y, 0);
		GX_Color1u32   (color);
		GX_TexCoord2f32(s1, t2);
	GX_End();
	GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp  (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_NONE);
}

/**
 * Call this function after drawing.
 */
void  GRRLIB_Render (void) {
	GX_DrawDone();          // Tell the GX engine we are done drawing
	GX_InvalidateTexAll();

	fb ^= 1;  // Toggle framebuffer index

	GX_SetZMode      (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp      (xfb[fb], GX_TRUE);

	VIDEO_SetNextFramebuffer(xfb[fb]);  // Select External Frame Buffer
	VIDEO_Flush();                      // Flush video buffer to screen

	if (!enable_output) // stfour: this prevent strange behavior on first frame
	{
		VIDEO_SetBlack(false);  // Enable video output
		enable_output = true;
	}

	VIDEO_WaitVSync();                  // Wait for screen to update
	// Interlaced screens require two frames to update
	if (rmode->viTVMode &VI_NON_INTERLACE)  VIDEO_WaitVSync();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_print
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Print formatted output.
 * @param xpos Specifies the x-coordinate of the upper-left corner of the text.
 * @param ypos Specifies the y-coordinate of the upper-left corner of the text.
 * @param tex The texture containing the character set.
 * @param color Text color in RGBA format. The alpha channel is used to change the opacity of the text.
 * @param zoom This is a factor by which the text size will be increase or decrease.
 * @param text Text to draw.
 * @param ... Optional arguments.
 */
void  GRRLIB_Printf (const f32 xpos, const f32 ypos,
					 const GRRLIB_texImg *tex, const u32 color,
					 const f32 zoom, const char *text, ...) {
	if (tex == NULL || tex->data == NULL) {
		return;
	}

	int i, size;
	char tmp[1024];
	f32 offset = tex->tilew * zoom;

	va_list argp;
	va_start(argp, text);
	size = vsnprintf(tmp, sizeof(tmp), text, argp);
	va_end(argp);

	for (i = 0; i < size; i++) {
		GRRLIB_DrawTile(xpos+i*offset, ypos, tex, 0, zoom, zoom, color,
			tmp[i] - tex->tilestart);
	}
}

/**
 * Print formatted output with a ByteMap font.
 * This function could be slow, it should be used with GRRLIB_CompoStart and GRRLIB_CompoEnd.
 * @param xpos Specifies the x-coordinate of the upper-left corner of the text.
 * @param ypos Specifies the y-coordinate of the upper-left corner of the text.
 * @param bmf The ByteMap font to use.
 * @param text Text to draw.
 * @param ... Optional arguments.
 */
void  GRRLIB_PrintBMF (const f32 xpos, const f32 ypos,
					   const GRRLIB_bytemapFont *bmf,
					   const char *text, ...) {
	uint  i, size;
	u8    *pdata;
	u8    x, y;
	char  tmp[1024];
	f32   xoff = xpos;
	const GRRLIB_bytemapChar *pchar;

	va_list argp;
	va_start(argp, text);
	size = vsnprintf(tmp, sizeof(tmp), text, argp);
	va_end(argp);

	for (i=0; i<size; i++) {
		pchar = &bmf->charDef[(u8)tmp[i]];
		pdata = pchar->data;
		for (y=0; y<pchar->height; y++) {
			for (x=0; x<pchar->width; x++) {
				if (*pdata) {
					GRRLIB_Plot(xoff + x + pchar->relx,
								ypos + y + pchar->rely,
								bmf->palette[*pdata]);
				}
				pdata++;
			}
		}
		xoff += pchar->kerning + bmf->tracking;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_gecko
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool geckoinit = false;

/**
 * Initialize USB Gecko.
 */
bool GRRLIB_GeckoInit() {
	u32 geckoattached = usb_isgeckoalive(EXI_CHANNEL_1);
	if (geckoattached) {
		usb_flush(EXI_CHANNEL_1);
		geckoinit = true;
		return true;
	}
	else return false;
}

/**
 * Print Gecko.
 * @param text Text to print.
 * @param ... Optional arguments.
 */
void  GRRLIB_GeckoPrintf (const char *text, ...) {
	int size;
	char tmp[1024];

	if (!geckoinit) return;

	va_list argp;
	va_start(argp, text);
	size = vsnprintf(tmp, sizeof(tmp), text, argp);
	va_end(argp);

	usb_sendbuffer_safe(1, tmp, size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_fileIO
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Load a file to memory.
 * @param filename Name of the file to be loaded.
 * @param data Pointer-to-your-pointer.
 * Ie. { u8 *data; GRRLIB_LoadFile("file", &data); }.
 * It is your responsibility to free the memory allocated by this function.
 * @return A integer representating a code:
 *         -     0 : EmptyFile.
 *         -    -1 : FileNotFound.
 *         -    -2 : OutOfMemory.
 *         -    -3 : FileReadError.
 *         -    >0 : FileLength.
 */
int  GRRLIB_LoadFile(const char* filename, unsigned char* *data) {
	int   len;
	FIL   fd;

	// Open the file
	if (f_open_char(&fd, filename, FA_READ|FA_OPEN_EXISTING) != FR_OK) {
		return -1;
	}

	// Get file length
	len = fd.obj.objsize;
	if (len == 0) {
		f_close(&fd);
		*data = NULL;
		return 0;
	}

	// Grab some memory in which to store the file
	if ( !(*data = malloc(len)) ) {
		f_close(&fd);
		return -2;
	}

	UINT read;
	if ( f_read(&fd, *data, len, &read) != FR_OK || read != len ) {
		f_close(&fd);
		free(*data);  *data = NULL;
		return -3;
	}

	f_close(&fd);
	return len;
}

/**
 * Load a texture from a file.
 * @param filename The JPEG, PNG or Bitmap filename to load.
 * @return A GRRLIB_texImg structure filled with image information.
 *         If an error occurs NULL will be returned.
 */
GRRLIB_texImg*  GRRLIB_LoadTextureFromFile(const char *filename) {
	GRRLIB_texImg  *tex;
	unsigned char  *data;

	// return NULL it load fails
	if (GRRLIB_LoadFile(filename, &data) <= 0)  return NULL;

	// Convert to texture
	tex = GRRLIB_LoadTexture(data);

	// Free up the buffer
	free(data);

	return tex;
}

/**
 * Make a PNG screenshot.
 * It should be called after drawing stuff on the screen, but before GRRLIB_Render.
 * libfat is required to use the function.
 * @param filename Name of the file to write.
 * @return bool true=everything worked, false=problems occurred.
 */
bool  GRRLIB_ScrShot(const char* filename) {
	IMGCTX  pngContext;
	int     ret = -1;

	if ( (pngContext = PNGU_SelectImageFromDevice(filename)) ) {
		ret = PNGU_EncodeFromEFB( pngContext,
								  rmode->fbWidth, rmode->efbHeight,
								  0 );
		PNGU_ReleaseImageContext(pngContext);
	}
	return !ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_fbAdvanced
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Draw a circle.
 * @author Dark_Link
 * @param x Specifies the x-coordinate of the circle.
 * @param y Specifies the y-coordinate of the circle.
 * @param radius The radius of the circle.
 * @param color The color of the circle in RGBA format.
 * @param filled Set to true to fill the circle.
 */
void  GRRLIB_Circle (const f32 x,  const f32 y,  const f32 radius,
					 const u32 color, const u8 filled) {
	guVector v[36];
	u32 ncolor[36];
	u32 a;
	f32 ra;
	f32 G_DTOR = M_DTOR * 10;

	for (a = 0; a < 36; a++) {
		ra = a * G_DTOR;

		v[a].x = cos(ra) * radius + x;
		v[a].y = sin(ra) * radius + y;
		v[a].z = 0.0f;
		ncolor[a] = color;
	}

	if (!filled)  GRRLIB_GXEngine(v, ncolor, 36, GX_LINESTRIP  );
	else          GRRLIB_GXEngine(v, ncolor, 36, GX_TRIANGLEFAN);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_bmfx
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Flip texture horizontal.
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture destination.
 */
void  GRRLIB_BMFX_FlipH (const GRRLIB_texImg *texsrc, GRRLIB_texImg *texdest) {
	unsigned int x, y, txtWidth = texsrc->w - 1;

	for (y = 0; y < texsrc->h; y++) {
		for (x = 0; x < texsrc->w; x++) {
			GRRLIB_SetPixelTotexImg(txtWidth - x, y, texdest,
				GRRLIB_GetPixelFromtexImg(x, y, texsrc));
		}
	}
}

/**
 * Flip texture vertical.
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture destination.
 */
void  GRRLIB_BMFX_FlipV (const GRRLIB_texImg *texsrc, GRRLIB_texImg *texdest) {
	unsigned int x, y, texHeight = texsrc->h - 1;

	for (y = 0; y < texsrc->h; y++) {
		for (x = 0; x < texsrc->w; x++) {
			GRRLIB_SetPixelTotexImg(x, texHeight - y, texdest,
				GRRLIB_GetPixelFromtexImg(x, y, texsrc));
		}
	}
}

/**
 * Change a texture to gray scale.
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture grayscaled destination.
 */
void  GRRLIB_BMFX_Grayscale (const GRRLIB_texImg *texsrc,
							 GRRLIB_texImg *texdest) {
	unsigned int x, y;
	u8 gray;
	u32 color;

	for (y = 0; y < texsrc->h; y++) {
		for (x = 0; x < texsrc->w; x++) {
			color = GRRLIB_GetPixelFromtexImg(x, y, texsrc);

			gray = ((R(color)* 77 +
					 G(color)*150 +
					 B(color)* 28 ) / 255);

			GRRLIB_SetPixelTotexImg(x, y, texdest,
				((gray << 24) | (gray << 16) | (gray << 8) | A(color)));
		}
	}
	GRRLIB_SetHandle(texdest, 0, 0);
}

/**
 * Change a texture to sepia (old photo style).
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture destination.
 * @author elisherer
 */
void  GRRLIB_BMFX_Sepia (const GRRLIB_texImg *texsrc, GRRLIB_texImg *texdest) {
	unsigned int  x, y;
	u16           sr, sg, sb;
	u32           color;

	for (y = 0; y < texsrc->h; y++) {
		for (x = 0; x < texsrc->w; x++) {
			color = GRRLIB_GetPixelFromtexImg(x, y, texsrc);
			sr = R(color)*0.393 + G(color)*0.769 + B(color)*0.189;
			sg = R(color)*0.349 + G(color)*0.686 + B(color)*0.168;
			sb = R(color)*0.272 + G(color)*0.534 + B(color)*0.131;
			if (sr>255) {
				sr=255;
			}
			if (sg>255) {
				sg=255;
			}
			if (sb>255) {
				sb=255;
			}
			GRRLIB_SetPixelTotexImg(x, y, texdest,
									RGBA(sr,sg,sb,A(color)));
		}
	}
	GRRLIB_SetHandle(texdest, 0, 0);
}
/**
 * Invert colors of the texture.
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture destination.
 */
void  GRRLIB_BMFX_Invert (const GRRLIB_texImg *texsrc, GRRLIB_texImg *texdest) {
	unsigned int x, y;
	u32 color;

	if (texsrc == NULL || texdest == NULL) return;

	for (y = 0; y < texsrc->h; y++) {
		for (x = 0; x < texsrc->w; x++) {
			color = GRRLIB_GetPixelFromtexImg(x, y, texsrc);
			GRRLIB_SetPixelTotexImg(x, y, texdest,
				((0xFFFFFF - (color >> 8 & 0xFFFFFF)) << 8)  | (color & 0xFF));
		}

	GRRLIB_FlushTex (texdest);
	}
}

/**
 * A texture effect (Blur).
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture destination.
 * @param factor The blur factor.
 */
void  GRRLIB_BMFX_Blur (const GRRLIB_texImg *texsrc,
							  GRRLIB_texImg *texdest, const u32 factor) {
	int numba = (1+(factor<<1))*(1+(factor<<1));
	u32 x, y;
	s32 k, l;
	int tmp;
	int newr, newg, newb, newa;
	u32 colours[numba];
	u32 thiscol;

	for (x = 0; x < texsrc->w; x++) {
		for (y = 0; y < texsrc->h; y++) {
			newr = 0;
			newg = 0;
			newb = 0;
			newa = 0;

			tmp = 0;
			thiscol = GRRLIB_GetPixelFromtexImg(x, y, texsrc);

			for (k = x - factor; k <= x + factor; k++) {
				for (l = y - factor; l <= y + factor; l++) {
					if (k < 0 || k >= texsrc->w || l < 0 || l >= texsrc->h) {
						colours[tmp] = thiscol;
					}
					else {
						colours[tmp] = GRRLIB_GetPixelFromtexImg(k, l, texsrc);
					}
					tmp++;
				}
			}

			for (tmp = 0; tmp < numba; tmp++) {
				newr += (colours[tmp] >> 24) & 0xFF;
				newg += (colours[tmp] >> 16) & 0xFF;
				newb += (colours[tmp] >> 8) & 0xFF;
				newa += colours[tmp] & 0xFF;
			}

			newr /= numba;
			newg /= numba;
			newb /= numba;
			newa /= numba;

			GRRLIB_SetPixelTotexImg(x, y, texdest, (newr<<24) | (newg<<16) | (newb<<8) | newa);
		}
	}
}

/**
 * A texture effect (Scatter).
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture destination.
 * @param factor The factor level of the effect.
 */
void  GRRLIB_BMFX_Scatter (const GRRLIB_texImg *texsrc,
								 GRRLIB_texImg *texdest, const u32 factor) {
	unsigned int x, y;
	u32 val1, val2;
	u32 val3, val4;
	int factorx2 = factor*2;

	for (y = 0; y < texsrc->h; y++) {
		for (x = 0; x < texsrc->w; x++) {
			val1 = x + (int) (factorx2 * (rand() / (RAND_MAX + 1.0))) - factor;
			val2 = y + (int) (factorx2 * (rand() / (RAND_MAX + 1.0))) - factor;

			if ((val1 >= texsrc->w) || (val2 >= texsrc->h)) {
			}
			else {
				val3 = GRRLIB_GetPixelFromtexImg(x, y, texsrc);
				val4 = GRRLIB_GetPixelFromtexImg(val1, val2, texsrc);
				GRRLIB_SetPixelTotexImg(x, y, texdest, val4);
				GRRLIB_SetPixelTotexImg(val1, val2, texdest, val3);
			}
		}
	}
}

/**
 * A texture effect (Pixelate).
 * @see GRRLIB_FlushTex
 * @param texsrc The texture source.
 * @param texdest The texture destination.
 * @param factor The factor level of the effect.
 */
void  GRRLIB_BMFX_Pixelate (const GRRLIB_texImg *texsrc,
								  GRRLIB_texImg *texdest, const u32 factor) {
	unsigned int x, y;
	unsigned int xx, yy;
	u32 rgb;

	for (x = 0; x < texsrc->w - 1 - factor; x += factor) {
		for (y = 0; y < texsrc->h - 1 - factor; y +=factor) {
			rgb = GRRLIB_GetPixelFromtexImg(x, y, texsrc);
			for (xx = x; xx < x + factor; xx++) {
				for (yy = y; yy < y + factor; yy++) {
					GRRLIB_SetPixelTotexImg(xx, yy, texdest, rgb);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_bmf
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Load a ByteMap font structure from a buffer.
 * File format version 1.1 is used, more information could be found at http://bmf.wz.cz/bmf-format.htm
 * @param my_bmf The ByteMap font buffer to load.
 * @return A GRRLIB_bytemapFont structure filled with BMF information.
 * @see GRRLIB_FreeBMF
 */
GRRLIB_bytemapFont*  GRRLIB_LoadBMF (const u8 my_bmf[] ) {
	GRRLIB_bytemapFont *fontArray = (struct GRRLIB_bytemapFont *)malloc(sizeof(GRRLIB_bytemapFont));
	u32 i, j = 1;
	u8 nbPalette, c;
	short int numcolpal;
	u16 nbPixels;

	if (fontArray != NULL && my_bmf[0]==0xE1 && my_bmf[1]==0xE6 && my_bmf[2]==0xD5 && my_bmf[3]==0x1A) {
		fontArray->version = my_bmf[4];
		fontArray->tracking = my_bmf[8];
		nbPalette = my_bmf[16];
		numcolpal = 3 * nbPalette;
		fontArray->palette = (u32 *)calloc(nbPalette + 1, sizeof(u32));
		for (i=0; i < numcolpal; i+=3) {
			fontArray->palette[j++] = ((((my_bmf[i+17]<<2)+3)<<24) | (((my_bmf[i+18]<<2)+3)<<16) | (((my_bmf[i+19]<<2)+3)<<8) | 0xFF);
		}
		j = my_bmf[17 + numcolpal];
		fontArray->name = (char *)calloc(j + 1, sizeof(char));
		memcpy(fontArray->name, &my_bmf[18 + numcolpal], j);
		j = 18 + numcolpal + j;
		fontArray->nbChar = (my_bmf[j] | my_bmf[j+1]<<8);
		memset(fontArray->charDef, 0, 256 * sizeof(GRRLIB_bytemapChar));
		j++;
		for (i=0; i < fontArray->nbChar; i++) {
			c = my_bmf[++j];
			fontArray->charDef[c].width = my_bmf[++j];
			fontArray->charDef[c].height = my_bmf[++j];
			fontArray->charDef[c].relx = my_bmf[++j];
			fontArray->charDef[c].rely = my_bmf[++j];
			fontArray->charDef[c].kerning = my_bmf[++j];
			nbPixels = fontArray->charDef[c].width * fontArray->charDef[c].height;
			fontArray->charDef[c].data = (u8 *)malloc(nbPixels);
			if (nbPixels && fontArray->charDef[c].data) {
				memcpy(fontArray->charDef[c].data, &my_bmf[++j], nbPixels);
				j += (nbPixels - 1);
			}
		}
	}
	return fontArray;
}

/**
 * Free memory allocated by ByteMap fonts.
 * @param bmf A GRRLIB_bytemapFont structure.
 */
void  GRRLIB_FreeBMF (GRRLIB_bytemapFont *bmf) {
	u16 i;

	for (i=0; i<256; i++) {
		if(bmf->charDef[i].data) {
			free(bmf->charDef[i].data);
		}
	}
	free(bmf->palette);
	free(bmf->name);
	free(bmf);
	bmf = NULL;
}

/**
 * Initialize a tile set.
 * @param tex The texture to initialize.
 * @param tilew Width of the tile.
 * @param tileh Height of the tile.
 * @param tilestart Offset for starting position (Used in fonts).
 */
void  GRRLIB_InitTileSet (GRRLIB_texImg *tex,
						  const uint tilew, const uint tileh,
						  const uint tilestart) {
	tex->tilew = tilew;
	tex->tileh = tileh;
	if (tilew) // Avoid division by zero
		tex->nbtilew = tex->w / tilew;
	if (tileh) // Avoid division by zero
		tex->nbtileh = tex->h / tileh;
	tex->tilestart = tilestart;
	tex->tiledtex = true;
	tex->ofnormaltexx = 1.0F / tex->nbtilew;
	tex->ofnormaltexy = 1.0F / tex->nbtileh;
	GRRLIB_SetHandle( tex, 0, 0 );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRRLIB_3D
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "grrlib.h"

// User should not directly modify these
Mtx       _GRR_view;  // Should be static as soon as all light functions needing this var will be in this file ;)
static  guVector  _GRR_cam  = {0.0F, 0.0F, 0.0F},
				  _GRR_up   = {0.0F, 1.0F, 0.0F},
				  _GRR_look = {0.0F, 0.0F, -100.0F};
static  guVector  _GRRaxisx = {1, 0, 0}; // DO NOT MODIFY!!!
static  guVector  _GRRaxisy = {0, 1, 0}; // Even at runtime
static  guVector  _GRRaxisz = {0, 0, 1}; // NOT ever!
static  Mtx 	  _ObjTransformationMtx;
/**
 * Set the background parameter when screen is cleared.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void GRRLIB_SetBackgroundColour(u8 r, u8 g, u8 b, u8 a) {
   GX_SetCopyClear((GXColor){ r, g, b, a }, GX_MAX_Z24);
}

/**
 * Set the camera parameter (contributed my chris_c aka DaShAmAn).
 * @param posx x position of the camera.
 * @param posy y position of the camera.
 * @param posz z position of the camera.
 * @param upx Alpha component.
 * @param upy Alpha component.
 * @param upz Alpha component.
 * @param lookx x up position of the camera.
 * @param looky y up position of the camera.
 * @param lookz z up position of the camera.
 */
void GRRLIB_Camera3dSettings(f32 posx, f32 posy, f32 posz,
	f32 upx, f32 upy, f32 upz,
	f32 lookx, f32 looky, f32 lookz) {

   _GRR_cam.x=posx;
   _GRR_cam.y=posy;
   _GRR_cam.z=posz;

   _GRR_up.x=upx;
   _GRR_up.y=upy;
   _GRR_up.z=upz;

   _GRR_look.x=lookx;
   _GRR_look.y=looky;
   _GRR_look.z=lookz;
}

/**
 * Set up the position matrix (contributed by chris_c aka DaShAmAn).
 * @param minDist Minimal distance for the camera.
 * @param maxDist Maximal distance for the camera.
 * @param fov Field of view for the camera.
 * @param texturemode False, GX won't need texture coordinate, True, GX will need texture coordinate.
 * @param normalmode False, GX won't need normal coordinate, True, GX will need normal coordinate.
 */
void GRRLIB_3dMode(f32 minDist, f32 maxDist, f32 fov, bool texturemode, bool normalmode) {
	Mtx m;

	guLookAt(_GRR_view, &_GRR_cam, &_GRR_up, &_GRR_look);
	guPerspective(m, fov, (f32)rmode->fbWidth/rmode->efbHeight, minDist, maxDist);
	GX_LoadProjectionMtx(m, GX_PERSPECTIVE);
	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);

	GX_SetCullMode(GX_CULL_NONE);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	if(normalmode)   GX_SetVtxDesc(GX_VA_NRM, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	if(texturemode)  GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	if(normalmode)   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	if(texturemode)  GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	if(texturemode)  GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	else             GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

/**
 * Go back to 2D mode (contributed by chris_c aka DaShAmAn).
 */
void GRRLIB_2dMode() {
	Mtx view, m;

	GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_TRUE);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	guOrtho(m, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 1000.0f);
	GX_LoadProjectionMtx(m, GX_ORTHOGRAPHIC);

	guMtxIdentity(view);
	guMtxTransApply(view, view, 0, 0, -100.0F);
	GX_LoadPosMtxImm(view, GX_PNMTX0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumTexGens(1);  // One texture exists
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetNumTevStages(1);

	GX_SetTevOp  (GX_TEVSTAGE0, GX_PASSCLR);

	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	GRRLIB_Settings.lights  = 0;
}

/**
 * Init the object matrix to draw object.
 */
void GRRLIB_ObjectViewBegin(void) {
	guMtxIdentity(_ObjTransformationMtx);
}

/**
 * Scale the object matrix to draw object.
 * @param scalx x scale of the object.
 * @param scaly y scale of the object.
 * @param scalz z scale of the object.
 */
void GRRLIB_ObjectViewScale(f32 scalx, f32 scaly, f32 scalz) {
	Mtx m;

	guMtxIdentity(m);
	guMtxScaleApply(m, m, scalx, scaly, scalz);

	guMtxConcat(m, _ObjTransformationMtx, _ObjTransformationMtx);
}

/**
 * Rotate the object matrix to draw object .
 * @param angx x rotation angle of the object.
 * @param angy y rotation angle of the object.
 * @param angz z rotation angle of the object.
 */
void GRRLIB_ObjectViewRotate(f32 angx, f32 angy, f32 angz) {
	Mtx m, rx,ry,rz;

	guMtxIdentity(m);
	guMtxRotAxisDeg(rx, &_GRRaxisx, angx);
	guMtxRotAxisDeg(ry, &_GRRaxisy, angy);
	guMtxRotAxisDeg(rz, &_GRRaxisz, angz);
	guMtxConcat(ry, rx, m);
	guMtxConcat(m, rz, m);

	guMtxConcat(m, _ObjTransformationMtx, _ObjTransformationMtx);
}

/**
 * Translate the object matrix to draw object.
 * @param posx x position of the object.
 * @param posy y position of the object.
 * @param posz z position of the object.
 */
void GRRLIB_ObjectViewTrans(f32 posx, f32 posy, f32 posz) {
	Mtx m;

	guMtxIdentity(m);
	guMtxTransApply(m, m, posx, posy, posz);

	guMtxConcat(m, _ObjTransformationMtx, _ObjTransformationMtx);
}

/**
 * Concat the object and the view matrix and calculate the inverse normal matrix.
 */
void GRRLIB_ObjectViewEnd(void) {
	Mtx mv, mvi;

	guMtxConcat(_GRR_view, _ObjTransformationMtx, mv);
	GX_LoadPosMtxImm(mv, GX_PNMTX0);

	guMtxInverse(mv, mvi);
	guMtxTranspose(mvi, mv);
	GX_LoadNrmMtxImm(mv, GX_PNMTX0);
}

/**
 * Set the view matrix to draw object (in this order scale, rotate AND trans).
 * @param posx x position of the object.
 * @param posy y position of the object.
 * @param posz z position of the object.
 * @param angx x rotation angle of the object.
 * @param angy y rotation angle of the object.
 * @param angz z rotation angle of the object.
 * @param scalx x scale of the object.
 * @param scaly y scale of the object.
 * @param scalz z scale of the object.
 */
void GRRLIB_ObjectView(f32 posx, f32 posy, f32 posz, f32 angx, f32 angy, f32 angz, f32 scalx, f32 scaly, f32 scalz) {
	Mtx ObjTransformationMtx;
	Mtx m, rx,ry,rz;
	Mtx mv, mvi;

	guMtxIdentity(ObjTransformationMtx);

	if((scalx !=1.0f) || (scaly !=1.0f) || (scalz !=1.0f)) {
		guMtxIdentity(m);
		guMtxScaleApply(m, m, scalx, scaly, scalz);

		guMtxConcat(m, ObjTransformationMtx, ObjTransformationMtx);
	}

	if((angx !=0.0f) || (angy !=0.0f) || (angz !=0.0f)) {
		guMtxIdentity(m);
		guMtxRotAxisDeg(rx, &_GRRaxisx, angx);
		guMtxRotAxisDeg(ry, &_GRRaxisy, angy);
		guMtxRotAxisDeg(rz, &_GRRaxisz, angz);
		guMtxConcat(ry, rx, m);
		guMtxConcat(m, rz, m);

		guMtxConcat(m, ObjTransformationMtx, ObjTransformationMtx);
	}

	if((posx !=0.0f) || (posy !=0.0f) || (posz !=0.0f)) {
		guMtxIdentity(m);
		guMtxTransApply(m, m, posx, posy, posz);

		guMtxConcat(m, ObjTransformationMtx, ObjTransformationMtx);
	}

	guMtxConcat(_GRR_view, ObjTransformationMtx, mv);
	GX_LoadPosMtxImm(mv, GX_PNMTX0);

	guMtxInverse(mv, mvi);
	guMtxTranspose(mvi, mv);
	GX_LoadNrmMtxImm(mv, GX_PNMTX0);
}

/**
 * Set the view matrix to draw object (in this order scale, trans AND rotate).
 * @param posx x position of the object.
 * @param posy y position of the object.
 * @param posz z position of the object.
 * @param angx x rotation angle of the object.
 * @param angy y rotation angle of the object.
 * @param angz z rotation angle of the object.
 * @param scalx x scale of the object.
 * @param scaly y scale of the object.
 * @param scalz z scale of the object.
 */
void GRRLIB_ObjectViewInv(f32 posx, f32 posy, f32 posz, f32 angx, f32 angy, f32 angz, f32 scalx, f32 scaly, f32 scalz) {
	Mtx ObjTransformationMtx;
	Mtx m, rx,ry,rz;
	Mtx mv, mvi;

	guMtxIdentity(ObjTransformationMtx);

	if((scalx !=1.0f) || (scaly !=1.0f) || (scalz !=1.0f)) {
		guMtxIdentity(m);
		guMtxScaleApply(m, m, scalx, scaly, scalz);

		guMtxConcat(m, ObjTransformationMtx, ObjTransformationMtx);
	}

	if((posx !=0.0f) || (posy !=0.0f) || (posz !=0.0f)) {
		guMtxIdentity(m);
		guMtxTransApply(m, m, posx, posy, posz);

		guMtxConcat(m, ObjTransformationMtx, ObjTransformationMtx);
	}

	if((angx !=0.0f) || (angy !=0.0f) || (angz !=0.0f)) {
		guMtxIdentity(m);
		guMtxRotAxisDeg(rx, &_GRRaxisx, angx);
		guMtxRotAxisDeg(ry, &_GRRaxisy, angy);
		guMtxRotAxisDeg(rz, &_GRRaxisz, angz);
		guMtxConcat(ry, rx, m);
		guMtxConcat(m, rz, m);

		guMtxConcat(m, ObjTransformationMtx, ObjTransformationMtx);
	}

	guMtxConcat(_GRR_view, ObjTransformationMtx, mv);
	GX_LoadPosMtxImm(mv, GX_PNMTX0);

	guMtxInverse(mv, mvi);
	guMtxTranspose(mvi, mv);
	GX_LoadNrmMtxImm(mv, GX_PNMTX0);
}

/**
 * Set the texture to an object (contributed by chris_c aka DaShAmAn).
 * @param tex Pointer to an image texture (GRRLIB_texImg format).
 * @param rep Texture Repeat Mode, True will repeat it, False won't.
 */
void GRRLIB_SetTexture(GRRLIB_texImg *tex, bool rep) {
	GXTexObj  texObj;

	if (rep) {
		GX_InitTexObj(&texObj, tex->data, tex->w, tex->h, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
	}
	else {
		GX_InitTexObj(&texObj, tex->data, tex->w, tex->h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	}
	if (GRRLIB_Settings.antialias == false) {
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
		GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	}
	else {
		GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	}

	GX_LoadTexObj(&texObj,      GX_TEXMAP0);
	GX_SetTevOp  (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc(GX_VA_TEX0,   GX_DIRECT);
}

/**
 * Draw a torus (with normal).
 * @param r Radius of the ring.
 * @param R Radius of the torus.
 * @param nsides Number of faces per ring.
 * @param rings Number of rings.
 * @param filled Wired or not.
 * @param col Color of the torus.
 */
void GRRLIB_DrawTorus(f32 r, f32 R, int nsides, int rings, bool filled, u32 col) {
	int i, j;
	f32 theta, phi, theta1;
	f32 cosTheta, sinTheta;
	f32 cosTheta1, sinTheta1;
	f32 ringDelta, sideDelta;
	f32 cosPhi, sinPhi, dist;

	ringDelta = 2.0 * M_PI / rings;
	sideDelta = 2.0 * M_PI / nsides;

	theta = 0.0;
	cosTheta = 1.0;
	sinTheta = 0.0;
	for (i = rings - 1; i >= 0; i--) {
		theta1 = theta + ringDelta;
		cosTheta1 = cos(theta1);
		sinTheta1 = sin(theta1);
		if(filled) GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 2*(nsides+1));
		else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 2*(nsides+1));
		phi = 0.0;
		for (j = nsides; j >= 0; j--) {
			phi += sideDelta;
			cosPhi = cos(phi);
			sinPhi = sin(phi);
			dist = R + r * cosPhi;

			GX_Position3f32(cosTheta1 * dist, -sinTheta1 * dist, r * sinPhi);
			GX_Normal3f32(cosTheta1 * cosPhi, -sinTheta1 * cosPhi, sinPhi);
			GX_Color1u32(col);
			GX_Position3f32(cosTheta * dist, -sinTheta * dist,  r * sinPhi);
			GX_Normal3f32(cosTheta * cosPhi, -sinTheta * cosPhi, sinPhi);
			GX_Color1u32(col);
		}
		GX_End();
		theta = theta1;
		cosTheta = cosTheta1;
		sinTheta = sinTheta1;
	}
}

/**
 * Draw a sphere (with normal).
 * @param r Radius of the sphere.
 * @param lats Number of lattitudes.
 * @param longs Number of longitutes.
 * @param filled Wired or not.
 * @param col Color of the sphere.
 */
void GRRLIB_DrawSphere(f32 r, int lats, int longs, bool filled, u32 col) {
	int i, j;
	f32 lat0, z0, zr0,
		lat1, z1, zr1,
		lng, x, y;

	for(i = 0; i <= lats; i++) {
		lat0 = M_PI * (-0.5F + (f32) (i - 1) / lats);
		z0  = sin(lat0);
		zr0 = cos(lat0);

		lat1 = M_PI * (-0.5F + (f32) i / lats);
		z1 = sin(lat1);
		zr1 = cos(lat1);
		if(filled) GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 2*(longs+1));
		else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 2*(longs+1));
		for(j = 0; j <= longs; j++) {
			lng = 2 * M_PI * (f32) (j - 1) / longs;
			x = cos(lng);
			y = sin(lng);

			GX_Position3f32(x * zr0 * r, y * zr0 * r, z0 * r);
			GX_Normal3f32(x * zr0 * r, y * zr0 * r, z0 * r);
			GX_Color1u32(col);
			GX_Position3f32(x * zr1 * r, y * zr1 * r, z1 * r);
			GX_Normal3f32(x * zr1 * r, y * zr1 * r, z1 * r);
			GX_Color1u32(col);
		}
		GX_End();
	}
}

/**
 * Draw a cube (with normal).
 * @param size Size of the cube edge.
 * @param filled Wired or not.
 * @param col Color of the cube.
 */
void GRRLIB_DrawCube(f32 size, bool filled, u32 col) {
	static f32 n[6][3] =
	{
		{-1.0, 0.0, 0.0},
		{0.0, 1.0, 0.0},
		{1.0, 0.0, 0.0},
		{0.0, -1.0, 0.0},
		{0.0, 0.0, 1.0},
		{0.0, 0.0, -1.0}
	};
	static int faces[6][4] =
	{
		{0, 1, 2, 3},
		{3, 2, 6, 7},
		{7, 6, 5, 4},
		{4, 5, 1, 0},
		{5, 6, 2, 1},
		{7, 4, 0, 3}
	};
	f32 v[8][3];
	int i;

	v[0][0] = v[1][0] = v[2][0] = v[3][0] = -size / 2;
	v[4][0] = v[5][0] = v[6][0] = v[7][0] = size / 2;
	v[0][1] = v[1][1] = v[4][1] = v[5][1] = -size / 2;
	v[2][1] = v[3][1] = v[6][1] = v[7][1] = size / 2;
	v[0][2] = v[3][2] = v[4][2] = v[7][2] = -size / 2;
	v[1][2] = v[2][2] = v[5][2] = v[6][2] = size / 2;

	for (i = 5; i >= 0; i--) {
		if(filled) GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 5);
		GX_Position3f32(v[faces[i][0]][0], v[faces[i][0]][1], v[faces[i][0]][2] );
		GX_Normal3f32(n[i][0], n[i][1], n[i][2]);
		GX_Color1u32(col);
		GX_Position3f32(v[faces[i][1]][0], v[faces[i][1]][1], v[faces[i][1]][2]);
		GX_Normal3f32(n[i][0], n[i][1], n[i][2]);
		GX_Color1u32(col);
		GX_Position3f32(v[faces[i][2]][0], v[faces[i][2]][1], v[faces[i][2]][2]);
		GX_Normal3f32(n[i][0], n[i][1], n[i][2]);
		GX_Color1u32(col);
		GX_Position3f32(v[faces[i][3]][0], v[faces[i][3]][1], v[faces[i][3]][2]);
		GX_Normal3f32(n[i][0], n[i][1], n[i][2]);
		GX_Color1u32(col);
		if(!filled) {
			GX_Position3f32(v[faces[i][0]][0], v[faces[i][0]][1], v[faces[i][0]][2]);
			GX_Normal3f32(n[i][0], n[i][1], n[i][2]);
			GX_Color1u32(col);
		}
		GX_End();
	}
}

/**
 * Draw a cylinder (with normal).
 * @param r Radius of the cylinder.
 * @param h High of the cylinder.
 * @param d Dencity of slice.
 * @param filled Wired or not.
 * @param col Color of the cylinder.
 */
void GRRLIB_DrawCylinder(f32 r, f32 h, int d, bool filled, u32 col) {
	int i;
	f32 dx, dy;

	if(filled) GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 2 * (d+1));
	else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 2 * (d+1));
	for(i = 0 ; i <= d ; i++) {
		dx = cosf( M_PI * 2.0f * i / d );
		dy = sinf( M_PI * 2.0f * i / d );
		GX_Position3f32( r * dx, -0.5f * h, r * dy );
		GX_Normal3f32( dx, 0.0f, dy );
		GX_Color1u32(col);
		GX_Position3f32( r * dx, 0.5f * h, r * dy );
		GX_Normal3f32( dx, 0.0f, dy );
		GX_Color1u32(col);
	}
	GX_End();

	if(filled) GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, d+2);
	else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, d+2);
	GX_Position3f32(0.0f, -0.5f * h, 0.0f);
	GX_Normal3f32(0.0f, -1.0f, 0.0f);
	GX_Color1u32(col);
	for(i = 0 ; i <= d ; i++) {
		GX_Position3f32( r * cosf( M_PI * 2.0f * i / d ), -0.5f * h, r * sinf( M_PI * 2.0f * i / d ) );
		GX_Normal3f32(0.0f, -1.0f, 0.0f);
		GX_Color1u32(col);
	}
	GX_End();

	if(filled) GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, d+2);
	else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, d+2);
	GX_Position3f32(0.0f, 0.5f * h, 0.0f);
	GX_Normal3f32(0.0f, 1.0f, 0.0f);
	GX_Color1u32(col);
	for(i = 0 ; i <= d ; i++) {
		GX_Position3f32( r * cosf( M_PI * 2.0f * i / d ), 0.5f * h, r * sinf( M_PI * 2.0f * i / d ) );
		GX_Normal3f32(0.0f, 1.0f, 0.0f);
		GX_Color1u32(col);
	}
	GX_End();
}

/**
 * Draw a cone (with normal).
 * @param r Radius of the cone.
 * @param h High of the cone.
 * @param d Dencity of slice.
 * @param filled Wired or not.
 * @param col Color of the cone.
 */
void GRRLIB_DrawCone(f32 r, f32 h, int d, bool filled, u32 col) {
	int i;
	f32 dx, dy;

	if(filled) GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 2 * (d+1));
	else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 2 * (d+1));
	for(i = 0 ; i <= d ; i++) {
		dx = cosf( M_PI * 2.0f * i / d );
		dy = sinf( M_PI * 2.0f * i / d );
		GX_Position3f32( 0, -0.5f * h,0);
		GX_Normal3f32( dx, 0.0f, dy );
		GX_Color1u32(col);
		GX_Position3f32( r * dx, 0.5f * h, r * dy );
		GX_Normal3f32( dx, 0.0f, dy );
		GX_Color1u32(col);
	}
	GX_End();

	if(filled) GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, d+2);
	else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, d+2);
	GX_Position3f32(0.0f, 0.5f * h, 0.0f);
	GX_Normal3f32(0.0f, 1.0f, 0.0f);
	GX_Color1u32(col);
	for(i = 0 ; i <= d ; i++) {
		GX_Position3f32( r * cosf( M_PI * 2.0f * i / d ), 0.5f * h, r * sinf( M_PI * 2.0f * i / d ) );
		GX_Normal3f32(0.0f, 1.0f, 0.0f);
		GX_Color1u32(col);
	}
	GX_End();
}

/**
 * Draw a Tesselated pannel (with normal).
 * @param w Width of the panel.
 * @param wstep Size of width slices.
 * @param h Height of the pannel.
 * @param hstep Size the de height slices.
 * @param filled Wired or not.
 * @param col Color in RGBA format.
 */
void GRRLIB_DrawTessPanel(f32 w, f32 wstep, f32 h, f32 hstep, bool filled, u32 col) {
 f32 x,y,tmpx, tmpy;
 int tmp;
 tmpy = h/2.0f;
 tmpx = w/2.0f;
 tmp = ((w/wstep)*2)+2;
	for ( y = -tmpy ; y <= tmpy ; y+=hstep )
	{
		if(filled) GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, tmp);
		else       GX_Begin(GX_LINESTRIP, GX_VTXFMT0, tmp);
			for ( x = -tmpx ; x <= tmpx ; x+=wstep )
			{
				GX_Position3f32( x, y, 0.0f );
				GX_Normal3f32( 0.0f, 0.0f, 1.0f);
				GX_Color1u32(col);
				GX_Position3f32( x, y+hstep, 0.0f );
				GX_Normal3f32( 0.0f, 0.0f, 1.0f);
				GX_Color1u32(col);
			}
		GX_End();
	}
}

/**
 * Set ambient color.
 * When no diffuse ligth is shinig on a object, the color is equal to ambient color.
 * @param ambientcolor Ambient color in RGBA format.
 */
void GRRLIB_SetLightAmbient(u32 ambientcolor) {
	GX_SetChanAmbColor(GX_COLOR0A0, (GXColor) { R(ambientcolor), G(ambientcolor), B(ambientcolor), 0xFF});
}

/**
 * Set diffuse light parameters.
 * @param num Number of the light. It's a number from 0 to 7.
 * @param pos Position of the diffuse light (x/y/z).
 * @param distattn Distance attenuation.
 * @param brightness Brightness of the light. The value should be between 0 and 1.
 * @param lightcolor Color of the light in RGBA format.
 */
void GRRLIB_SetLightDiff(u8 num, guVector pos, f32 distattn, f32 brightness, u32 lightcolor) {
	GXLightObj MyLight;
	guVector lpos = {pos.x, pos.y, pos.z};

	GRRLIB_Settings.lights |= (1<<num);

	guVecMultiply(_GRR_view, &lpos, &lpos);
	GX_InitLightPos(&MyLight, lpos.x, lpos.y, lpos.z);
	GX_InitLightColor(&MyLight, (GXColor) { R(lightcolor), G(lightcolor), B(lightcolor), 0xFF });
	GX_InitLightSpot(&MyLight, 0.0f, GX_SP_OFF);
	GX_InitLightDistAttn(&MyLight, distattn, brightness, GX_DA_MEDIUM); // DistAttn = 20.0  &  Brightness=1.0f (full)
	GX_LoadLightObj(&MyLight, (1<<num));

	// Turn light ON
	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_ENABLE, GX_SRC_REG, GX_SRC_VTX, GRRLIB_Settings.lights, GX_DF_CLAMP, GX_AF_SPOT);
}

/**
 * Set specular light parameters.
 * @param num Number of the light. It's a number from 0 to 7.
 * @param dir Direction of the specular ray (x/y/z).
 * @param shy Shyniness of the specular. ( between 4 and 254)
 * @param lightcolor Color of the light in RGBA format.
 * @param speccolor Specular color in RGBA format..
 */
void GRRLIB_SetLightSpec(u8 num, guVector dir, f32 shy, u32 lightcolor, u32 speccolor) {
	Mtx mr,mv;
	GXLightObj MyLight;
	guVector ldir = {dir.x, dir.y, dir.z};

	GRRLIB_Settings.lights |= (1<<num);

	guMtxInverse(_GRR_view,mr);
	guMtxTranspose(mr,mv);
	guVecMultiplySR(mv, &ldir,&ldir);
	GX_InitSpecularDirv(&MyLight, &ldir);

	GX_InitLightShininess(&MyLight, shy);  // between 4 and 255 !!!
	GX_InitLightColor(&MyLight, (GXColor) { R(lightcolor), G(lightcolor), B(lightcolor), 0xFF });
	GX_LoadLightObj(&MyLight, (1<<num));

	/////////////////////// Turn light ON ////////////////////////////////////////////////
	GX_SetNumChans(2);    // use two color channels
	GX_SetChanCtrl(GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT0, GX_DF_CLAMP, GX_AF_NONE);
	GX_SetChanCtrl(GX_COLOR1, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0, GX_DF_NONE, GX_AF_SPEC);
	GX_SetChanCtrl(GX_ALPHA0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
	GX_SetChanCtrl(GX_ALPHA1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);


	GX_SetNumTevStages(2);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0 );
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR1A1 );
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV );
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_RASC, GX_CC_ONE, GX_CC_CPREV );

	/////////////////////// Define Material and Ambiant color and draw object /////////////////////////////////////
	GX_SetChanAmbColor(GX_COLOR1, (GXColor){0x00,0x00,0x00,0xFF});  // specualr ambient forced to black
	GX_SetChanMatColor(GX_COLOR1, (GXColor) { R(speccolor), G(speccolor), B(speccolor), 0xFF }); // couleur du reflet specular
}

/**
 * Set Spot light parameters.
 * @param num Number of the light. It's a number from 0 to 7.
 * @param pos Position of the spot light (x/y/z).
 * @param lookat Where spot light look at (x/y/z).
 * @param angAttn0 cone attenuation factor 0.
 * @param angAttn1 cone attenuation factor 1.
 * @param angAttn2 cone attenuation factor 2.
 * @param distAttn0 Distance attenuation factor 0.
 * @param distAttn1 Distance attenuation factor 1.
 * @param distAttn2 Distance attenuation factor 2.
 * @param lightcolor Color of the light in RGBA format.
 */
void GRRLIB_SetLightSpot(u8 num, guVector pos, guVector lookat, f32 angAttn0, f32 angAttn1, f32 angAttn2, f32 distAttn0, f32 distAttn1, f32 distAttn2, u32 lightcolor) {
	GXLightObj lobj;
	guVector lpos = (guVector){ pos.x, pos.y, pos.z };
	guVector ldir = (guVector){ lookat.x-pos.x, lookat.y-pos.y, lookat.z-pos.z };
	guVecNormalize(&ldir);

	GRRLIB_Settings.lights |= (1<<num);

	guVecMultiplySR(_GRR_view, &ldir,&ldir);
	guVecMultiply(_GRR_view, &lpos, &lpos);

	GX_InitLightDirv(&lobj, &ldir);
	GX_InitLightPosv(&lobj, &lpos);
	GX_InitLightColor(&lobj, (GXColor) { R(lightcolor), G(lightcolor), B(lightcolor), 0xFF });

	//this is just for code readers, wanting to know how to use direct cut off
	//GX_InitLightSpot(&lobj, 0<angle<90, GX_SP_FLAT);

	GX_InitLightAttn(&lobj, angAttn0, angAttn1, angAttn2, distAttn0, distAttn1, distAttn2);

	GX_LoadLightObj(&lobj, (1<<num));

	// Turn light ON
	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_ENABLE, GX_SRC_REG, GX_SRC_VTX, GRRLIB_Settings.lights, GX_DF_CLAMP, GX_AF_SPOT);
}

/**
 * Set all lights off, like at init.
 */
void GRRLIB_SetLightOff(void) {
	GX_SetNumTevStages(1);

	GX_SetTevOp  (GX_TEVSTAGE0, GX_PASSCLR);

	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	GRRLIB_Settings.lights  = 0;
}
