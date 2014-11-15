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

/**
 * @file GRRLIB.h
 * GRRLIB user include file.
 */
/**
 * @defgroup AllFunc Everything in GRRLIB
 * This is the complete list of funtions, structures, defines, typedefs, enumerations and variables you may want to used to make your homebrew with GRRLIB.
 * You simply need to include grrlib.h in your project to have access to all of these.
 * @{
 */

#ifndef __GRRLIB_H__
#define __GRRLIB_H__

/**
 * Version information for GRRLIB.
 */
#define GRRLIB_VER_STRING "4.3.1"

//==============================================================================
// Includes
//==============================================================================
#include <gccore.h>
//==============================================================================

//==============================================================================
// C++ header
//==============================================================================
#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

//==============================================================================
// Extra standard declarations
//==============================================================================
typedef  unsigned int  uint;/**< The uint keyword signifies an integral type. */

//==============================================================================
// Primitive colour macros
//==============================================================================
#define R(c)  (((c) >>24) &0xFF)  /**< Exract Red   component of colour. */
#define G(c)  (((c) >>16) &0xFF)  /**< Exract Green component of colour. */
#define B(c)  (((c) >> 8) &0xFF)  /**< Exract Blue  component of colour. */
#define A(c)  ( (c)       &0xFF)  /**< Exract Alpha component of colour. */

/**
 * Build an RGB pixel from components.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
#define RGBA(r,g,b,a) ( (u32)( ( ((u32)(r))        <<24) |  \
                               ((((u32)(g)) &0xFF) <<16) |  \
                               ((((u32)(b)) &0xFF) << 8) |  \
                               ( ((u32)(a)) &0xFF      ) ) )

//==============================================================================
// typedefs, enumerators & structs
//==============================================================================
/**
 * GRRLIB Blending Modes.
 */
typedef  enum GRRLIB_blendMode {
    GRRLIB_BLEND_ALPHA  = 0,    /**< Alpha Blending.        */
    GRRLIB_BLEND_ADD    = 1,    /**< Additive Blending.     */
    GRRLIB_BLEND_SCREEN = 2,    /**< Alpha Light Blending.  */
    GRRLIB_BLEND_MULTI  = 3,    /**< Multiply Blending.     */
    GRRLIB_BLEND_INV    = 4,    /**< Invert Color Blending. */
} GRRLIB_blendMode;

#define GRRLIB_BLEND_NONE   (GRRLIB_BLEND_ALPHA)    /**< Alias for GRRLIB_BLEND_ALPHA. */
#define GRRLIB_BLEND_LIGHT  (GRRLIB_BLEND_ADD)      /**< Alias for GRRLIB_BLEND_ADD. */
#define GRRLIB_BLEND_SHADE  (GRRLIB_BLEND_MULTI)    /**< Alias for GRRLIB_BLEND_MULTI. */

//------------------------------------------------------------------------------
/**
 * Structure to hold the current drawing settings.
 */
typedef  struct GRRLIB_drawSettings {
    bool              antialias;    /**< AntiAlias is enabled when set to true. */
    GRRLIB_blendMode  blend;        /**< Blending Mode.                         */
    int               lights;       /**< Active lights.                         */
} GRRLIB_drawSettings;

//------------------------------------------------------------------------------
/**
 * Structure to hold the texture information.
 */
typedef  struct GRRLIB_texImg {
    uint   w;           /**< The width of the texture in pixels.  */
    uint   h;           /**< The height of the texture in pixels. */
    int    handlex;     /**< Texture handle x. */
    int    handley;     /**< Texture handle y. */
    int    offsetx;     /**< Texture offset x. */
    int    offsety;     /**< Texture offset y. */

    bool   tiledtex;    /**< Texture is tiled if set to true.   */
    uint   tilew;       /**< The width of one tile in pixels.   */
    uint   tileh;       /**< The height of one tile in pixels.  */
    uint   nbtilew;     /**< Number of tiles for the x axis.    */
    uint   nbtileh;     /**< Number of tiles for the y axis.    */
    uint   tilestart;   /**< Offset to tile starting position.  */
    f32    ofnormaltexx;/**< Offset of normalized texture on x. */
    f32    ofnormaltexy;/**< Offset of normalized texture on y. */

    void  *data;        /**< Pointer to the texture data. */
} GRRLIB_texImg;

//------------------------------------------------------------------------------
/**
 * Structure to hold the bytemap character information.
 */
typedef  struct GRRLIB_bytemapChar {
    u8  width;          /**< Character width.    */
    u8  height;         /**< Character height.   */
    s8  relx;           /**< Horizontal offset relative to cursor (-128 to 127).            */
    s8  rely;           /**< Vertical offset relative to cursor (-128 to 127).              */
    u8  kerning;        /**< Kerning (Horizontal cursor shift after drawing the character). */
    u8  *data;          /**< Character data (uncompressed, 8 bits per pixel).               */
} GRRLIB_bytemapChar;

//------------------------------------------------------------------------------
/**
 * Structure to hold the bytemap font information.
 */
typedef  struct GRRLIB_bytemapFont {
    char  *name;                /**< Font name.                      */
    u32   *palette;             /**< Font palette.                   */
    u16   nbChar;               /**< Number of characters in font.   */
    u8    version;              /**< Version.                        */
    s8    tracking;             /**< Tracking (Add-space after each char) (-128 to 127). */

    GRRLIB_bytemapChar charDef[256];   /**< Array of bitmap characters. */
} GRRLIB_bytemapFont;

//------------------------------------------------------------------------------
/**
 * Structure to hold the TTF information.
 */
typedef  struct GRRLIB_Font {
    void *face;     /**< A TTF face object. */
    bool kerning;   /**< true whenever a face object contains kerning data that can be accessed with FT_Get_Kerning. */
} GRRLIB_ttfFont;

//==============================================================================
// Allow general access to screen and frame information
//==============================================================================
#if defined __GRRLIB_CORE__
# define GRR_EXTERN
# define GRR_INIT(v)     = v
# define GRR_INITS(...)  = { __VA_ARGS__ }
#else
# define GRR_EXTERN      extern
# define GRR_INIT(v)
# define GRR_INITS(...)
#endif

GRR_EXTERN  GXRModeObj  *rmode;
GRR_EXTERN  void        *xfb[2]  GRR_INITS(NULL, NULL);
GRR_EXTERN  u32         fb       GRR_INIT(0);
//==============================================================================
// procedure and function prototypes
// Inline function handling - http://www.greenend.org.uk/rjk/2003/03/inline.html
//==============================================================================

//==========================================================================================================================================================================================================================================
// #include "grrlib/GRRLIB__lib.h"
//==========================================================================================================================================================================================================================================

#ifndef __GRRLIB_FNLIB_H__
#define __GRRLIB_FNLIB_H__

//==============================================================================
// Prototypes for library contained functions
//==============================================================================

//------------------------------------------------------------------------------
// GRRLIB_bmf.c - BitMapFont functions
GRRLIB_bytemapFont*  GRRLIB_LoadBMF (const u8 my_bmf[] );
void                 GRRLIB_FreeBMF (GRRLIB_bytemapFont *bmf);

void  GRRLIB_InitTileSet  (GRRLIB_texImg *tex,
                           const uint tilew, const uint tileh,
                           const uint tilestart);

//------------------------------------------------------------------------------
// GRRLIB_bmfx.c - Bitmap f/x
void  GRRLIB_BMFX_FlipH     (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest);

void  GRRLIB_BMFX_FlipV     (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest);

void  GRRLIB_BMFX_Grayscale (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest);

void  GRRLIB_BMFX_Sepia     (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest);

void  GRRLIB_BMFX_Invert    (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest);

void  GRRLIB_BMFX_Blur      (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest, const u32 factor);

void  GRRLIB_BMFX_Scatter   (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest, const u32 factor);

void  GRRLIB_BMFX_Pixelate  (const GRRLIB_texImg *texsrc,
                             GRRLIB_texImg *texdest, const u32 factor);

//------------------------------------------------------------------------------
// GRRLIB_core.c - GRRLIB core functions
int  GRRLIB_Init (void);
void  GRRLIB_Exit (void);
void  GRRLIB_ExitLight (void);

//------------------------------------------------------------------------------
// GRRLIB_fbAdvanced.c - Render to framebuffer: Advanced primitives
void  GRRLIB_Circle (const f32 x,  const f32 y,  const f32 radius,
                     const u32 color, const u8 filled);

//------------------------------------------------------------------------------
// GRRLIB_fileIO - File I/O (SD Card)
int             GRRLIB_LoadFile            (const char* filename,
                                            unsigned char* *data);
GRRLIB_texImg*  GRRLIB_LoadTextureFromFile (const char* filename);
bool            GRRLIB_ScrShot             (const char* filename);

//------------------------------------------------------------------------------
// GRRLIB_print.c - Will someone please tell me what these are :)
void  GRRLIB_Printf   (const f32 xpos, const f32 ypos,
                       const GRRLIB_texImg *tex, const u32 color,
                       const f32 zoom, const char *text, ...);

void  GRRLIB_PrintBMF (const f32 xpos, const f32 ypos,
                       const GRRLIB_bytemapFont *bmf,
                       const char *text, ...);

//------------------------------------------------------------------------------
// GRRLIB_render.c - Rendering functions
void  GRRLIB_DrawImg  (const f32 xpos, const f32 ypos, const GRRLIB_texImg *tex,
                       const f32 degrees, const f32 scaleX, const f32 scaleY,
                       const u32 color);

void  GRRLIB_DrawImgQuad  (const guVector pos[4], GRRLIB_texImg *tex,
                           const u32 color);

void  GRRLIB_DrawTile (const f32 xpos, const f32 ypos, const GRRLIB_texImg *tex,
                       const f32 degrees, const f32 scaleX, const f32 scaleY,
                       const u32 color, const int frame);

void  GRRLIB_DrawPart (const f32 xpos, const f32 ypos, const f32 partx, const f32 party,
                       const f32 partw, const f32 parth, const GRRLIB_texImg *tex,
                       const f32 degrees, const f32 scaleX, const f32 scaleY,
                       const u32 color);

void  GRRLIB_DrawTileQuad (const guVector pos[4], GRRLIB_texImg *tex, const u32 color, const int frame);

void  GRRLIB_Render  (void);

//------------------------------------------------------------------------------
// GRRLIB_snapshot.c - Create a texture containing a snapshot of a part of the framebuffer
void  GRRLIB_Screen2Texture (int posx, int posy, GRRLIB_texImg *tex, bool clear);
void  GRRLIB_CompoStart (void);
void  GRRLIB_CompoEnd(int posx, int posy, GRRLIB_texImg *tex);

//------------------------------------------------------------------------------
// GRRLIB_texEdit.c - Modifying the content of a texture
GRRLIB_texImg*  GRRLIB_LoadTexture    (const u8 *my_img);
GRRLIB_texImg*  GRRLIB_LoadTexturePNG (const u8 *my_png);
GRRLIB_texImg*  GRRLIB_LoadTextureJPG (const u8 *my_jpg);
GRRLIB_texImg*  GRRLIB_LoadTextureJPGEx (const u8 *my_jpg, const int);
GRRLIB_texImg*  GRRLIB_LoadTextureBMP (const u8 *my_bmp);

//------------------------------------------------------------------------------
// GRRLIB_gecko.c - USB_Gecko output facilities
bool GRRLIB_GeckoInit();
void GRRLIB_GeckoPrintf (const char *text, ...);

//------------------------------------------------------------------------------
// GRRLIB_3D.c - 3D functions for GRRLIB
void GRRLIB_SetBackgroundColour(u8 r, u8 g, u8 b, u8 a);
void GRRLIB_Camera3dSettings(f32 posx, f32 posy, f32 posz, f32 upx, f32 upy, f32 upz, f32 lookx, f32 looky, f32 lookz);
void GRRLIB_3dMode(f32 minDist, f32 maxDist, f32 fov, bool texturemode, bool normalmode);
void GRRLIB_2dMode();
void GRRLIB_ObjectViewBegin(void);
void GRRLIB_ObjectViewScale(f32 scalx, f32 scaly, f32 scalz);
void GRRLIB_ObjectViewRotate(f32 angx, f32 angy, f32 angz);
void GRRLIB_ObjectViewTrans(f32 posx, f32 posy, f32 posz);
void GRRLIB_ObjectViewEnd(void);
void GRRLIB_ObjectView(f32 posx, f32 posy, f32 posz, f32 angx, f32 angy, f32 angz,  f32 scalx, f32 scaly, f32 scalz);
void GRRLIB_ObjectViewInv(f32 posx, f32 posy, f32 posz, f32 angx, f32 angy, f32 angz,  f32 scalx, f32 scaly, f32 scalz);
void GRRLIB_SetTexture(GRRLIB_texImg *tex, bool rep);
void GRRLIB_DrawTorus(f32 r, f32 R, int nsides, int rings, bool filled, u32 col);
void GRRLIB_DrawSphere(f32 r, int lats, int longs, bool filled, u32 col);
void GRRLIB_DrawCube(f32 size, bool filled, u32 col);
void GRRLIB_DrawCylinder(f32 r, f32 h, int d, bool filled, u32 col);
void GRRLIB_DrawCone(f32 r, f32 h, int d, bool filled, u32 col);
void GRRLIB_DrawTessPanel(f32 w, f32 wstep, f32 h, f32 hstep, bool filled, u32 col);
void GRRLIB_SetLightAmbient(u32 ambientcolor);
void GRRLIB_SetLightDiff(u8 num, guVector pos, f32 distattn, f32 brightness, u32 lightcolor);
void GRRLIB_SetLightSpec(u8 num, guVector dir, f32 shy, u32 lightcolor, u32 speccolor);
void GRRLIB_SetLightSpot(u8 num, guVector pos, guVector lookat, f32 angAttn0, f32 angAttn1, f32 angAttn2, f32 distAttn0, f32 distAttn1, f32 distAttn2, u32 lightcolor);
void GRRLIB_SetLightOff(void);

//------------------------------------------------------------------------------
// GRRLIB_ttf.c - FreeType function for GRRLIB
GRRLIB_ttfFont* GRRLIB_LoadTTF(const u8* file_base, s32 file_size);
void GRRLIB_FreeTTF(GRRLIB_ttfFont *myFont);
void GRRLIB_PrintfTTF(int x, int y, GRRLIB_ttfFont *myFont, const char *string, unsigned int fontSize, const u32 color);
void GRRLIB_PrintfTTFW(int x, int y, GRRLIB_ttfFont *myFont, const wchar_t *string, unsigned int fontSize, const u32 color);
unsigned int GRRLIB_WidthTTF(GRRLIB_ttfFont *myFont, const char *, unsigned int);
unsigned int GRRLIB_WidthTTFW(GRRLIB_ttfFont *myFont, const wchar_t *, unsigned int);

#endif // __GRRLIB_FNLIB_H__

//==========================================================================================================================================================================================================================================
#if defined __GRRLIB_CORE__
#  define INLINE
#else
# if __GNUC__ && !__GNUC_STDC_INLINE__
#  define INLINE static inline
# else
#  define INLINE inline
# endif
#endif

//==========================================================================================================================================================================================================================================
// #include "grrlib/GRRLIB__inline.h"
//==========================================================================================================================================================================================================================================

#ifndef __GRRLIB_FNINLINE_H__
#define __GRRLIB_FNINLINE_H__

//==============================================================================
// Prototypes for inlined functions
//==============================================================================

//------------------------------------------------------------------------------
// GRRLIB_cExtn.h - C extensions (helper functions)
INLINE  u8    GRRLIB_ClampVar8 (f32 Value);

//------------------------------------------------------------------------------
// GRRLIB_clipping.h - Clipping control
INLINE  void  GRRLIB_ClipReset   (void);
INLINE  void  GRRLIB_ClipDrawing (const int x, const int y,
                                  const int width, const int height);

//------------------------------------------------------------------------------
// GRRLIB_collision.h - Collision detection
INLINE  bool  GRRLIB_PtInRect   (const int hotx,   const int hoty,
                                 const int hotw,   const int hoth,
                                 const int wpadx,  const int wpady);

INLINE  bool  GRRLIB_RectInRect (const int rect1x, const int rect1y,
                                 const int rect1w, const int rect1h,
                                 const int rect2x, const int rect2y,
                                 const int rect2w, const int rect2h);

INLINE  bool  GRRLIB_RectOnRect (const int rect1x, const int rect1y,
                                 const int rect1w, const int rect1h,
                                 const int rect2x, const int rect2y,
                                 const int rect2w, const int rect2h);

//------------------------------------------------------------------------------
// GRRLIB_fbComplex.h -
INLINE  void  GRRLIB_NPlot       (const guVector v[], const u32 color[],
                                  const long n);
INLINE  void  GRRLIB_NGone       (const guVector v[], const u32 color[],
                                  const long n);
INLINE  void  GRRLIB_NGoneFilled (const guVector v[], const u32 color[],
                                  const long n);

//------------------------------------------------------------------------------
// GRRLIB_fbGX.h -
INLINE  void  GRRLIB_GXEngine (const guVector v[], const u32 color[],
                               const long n,       const u8 fmt);

//------------------------------------------------------------------------------
// GRRLIB_fbSimple.h -
INLINE  void  GRRLIB_FillScreen (const u32 color);
INLINE  void  GRRLIB_Plot       (const f32 x,  const f32 y, const u32 color);
INLINE  void  GRRLIB_Line       (const f32 x1, const f32 y1,
                                 const f32 x2, const f32 y2, const u32 color);
INLINE  void  GRRLIB_Rectangle  (const f32 x,      const f32 y,
                                 const f32 width,  const f32 height,
                                 const u32 color, const u8 filled);

//------------------------------------------------------------------------------
// GRRLIB_handle.h - Texture handle manipulation
INLINE  void  GRRLIB_SetHandle (GRRLIB_texImg *tex, const int x, const int y);
INLINE  void  GRRLIB_SetMidHandle (GRRLIB_texImg *tex, const bool enabled);

//------------------------------------------------------------------------------
// GRRLIB_pixel.h - Pixel manipulation
INLINE  u32   GRRLIB_GetPixelFromtexImg (const int x, const int y,
                                         const GRRLIB_texImg *tex);

INLINE  void  GRRLIB_SetPixelTotexImg   (const int x, const int y,
                                         GRRLIB_texImg *tex, const u32 color);

INLINE u32 GRRLIB_GetPixelFromFB (int x, int y);
INLINE void GRRLIB_SetPixelToFB (int x, int y, u32 pokeColor);

//------------------------------------------------------------------------------
// GRRLIB_settings.h - Rendering functions
INLINE  void              GRRLIB_SetBlend        (const GRRLIB_blendMode blendmode);
INLINE  GRRLIB_blendMode  GRRLIB_GetBlend        (void);
INLINE  void              GRRLIB_SetAntiAliasing (const bool aa);
INLINE  bool              GRRLIB_GetAntiAliasing (void);

//------------------------------------------------------------------------------
// GRRLIB_texSetup.h - Create and setup textures
INLINE  GRRLIB_texImg*  GRRLIB_CreateEmptyTexture (const uint w, const uint h);
INLINE  void            GRRLIB_ClearTex           (GRRLIB_texImg* tex);
INLINE  void            GRRLIB_FlushTex           (GRRLIB_texImg *tex);
INLINE  void            GRRLIB_FreeTexture        (GRRLIB_texImg *tex);

//==============================================================================
// Definitions of inlined functions
//==============================================================================
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_texSetup.h
 * Inline functions for the basic manipulation of textures.
 */


/**
 * Create an empty texture.
 * @param w Width of the new texture to create.
 * @param h Height of the new texture to create.
 * @return A GRRLIB_texImg structure newly created.
 */
INLINE
GRRLIB_texImg*  GRRLIB_CreateEmptyTexture (const uint w, const uint h)
{
    GRRLIB_texImg *my_texture = (struct GRRLIB_texImg *)calloc(1, sizeof(GRRLIB_texImg));

    if(my_texture != NULL) {
        my_texture->data = memalign(32, h * w * 4);
        my_texture->w = w;
        my_texture->h = h;

        // Initialize the texture
        memset(my_texture->data, '\0', (h * w) << 2);

        GRRLIB_SetHandle(my_texture, 0, 0);
        GRRLIB_FlushTex(my_texture);
    }
    return my_texture;
}

/**
 * Write the contents of a texture in the data cache down to main memory.
 * For performance the CPU holds a data cache where modifications are stored before they get written down to main memory.
 * @param tex The texture to flush.
 */
INLINE
void  GRRLIB_FlushTex (GRRLIB_texImg *tex) 
	{
	if(tex != NULL)
    DCFlushRange(tex->data, tex->w * tex->h * 4);
	}

/**
 * Free memory allocated for texture.
 * @param tex A GRRLIB_texImg structure.
 */
INLINE
void  GRRLIB_FreeTexture (GRRLIB_texImg *tex) {
    if(tex != NULL) {
        if (tex->data != NULL)  free(tex->data);
        free(tex);
        tex = NULL;
    }
}

/**
 * Clear a texture to transparent black.
 * @param tex Texture to clear.
 */
INLINE
void  GRRLIB_ClearTex(GRRLIB_texImg* tex) {
    bzero(tex->data, (tex->h * tex->w) << 2);
    GRRLIB_FlushTex(tex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_settings.h
 * Inline functions for configuring the GRRLIB settings.
 */

#ifndef GX_BM_SUBTRACT
    /**
     * Blending type.
     * libogc revision 4170 fixed a typo. GX_BM_SUBSTRACT was renamed GX_BM_SUBTRACT.
     * But for previous versions this define is needed.
     */
    #define GX_BM_SUBTRACT      GX_BM_SUBSTRACT
#endif

extern  GRRLIB_drawSettings  GRRLIB_Settings;

/**
 * Set a blending mode.
 * @param blendmode The blending mode to use (Default: GRRLIB_BLEND_ALPHA).
 */
INLINE
void  GRRLIB_SetBlend (const GRRLIB_blendMode blendmode) {
    GRRLIB_Settings.blend = blendmode;
    switch (GRRLIB_Settings.blend) {
        case GRRLIB_BLEND_ALPHA:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
            break;
        case GRRLIB_BLEND_ADD:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_DSTALPHA, GX_LO_CLEAR);
            break;
        case GRRLIB_BLEND_SCREEN:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCCLR, GX_BL_DSTALPHA, GX_LO_CLEAR);
            break;
        case GRRLIB_BLEND_MULTI:
            GX_SetBlendMode(GX_BM_SUBTRACT, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
            break;
        case GRRLIB_BLEND_INV:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_INVSRCCLR, GX_BL_INVSRCCLR, GX_LO_CLEAR);
            break;
    }
}

/**
 * Get the current blending mode.
 * @return The current blending mode.
 */
INLINE
GRRLIB_blendMode  GRRLIB_GetBlend (void) {
    return GRRLIB_Settings.blend;
}

/**
 * Turn anti-aliasing on/off.
 * @param aa Set to true to enable anti-aliasing (Default: Enabled).
 */
INLINE
void  GRRLIB_SetAntiAliasing (const bool aa) {
    GRRLIB_Settings.antialias = aa;
}

/**
 * Get current anti-aliasing setting.
 * @return True if anti-aliasing is enabled.
 */
INLINE
bool  GRRLIB_GetAntiAliasing (void) {
    return GRRLIB_Settings.antialias;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_pixel.h
 * Inline functions for manipulating pixels in textures.
 */

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

/**
 * Return the color value of a pixel from a GRRLIB_texImg.
 * @param x Specifies the x-coordinate of the pixel in the texture.
 * @param y Specifies the y-coordinate of the pixel in the texture.
 * @param tex The texture to get the color from.
 * @return The color of a pixel in RGBA format.
 */
INLINE
u32  GRRLIB_GetPixelFromtexImg (const int x, const int y,
                                const GRRLIB_texImg *tex) {
    register u32  offs;
    register u32  ar;
    register u8*  bp = (u8*)tex->data;

    offs = (((y&(~3))<<2)*tex->w) + ((x&(~3))<<4) + ((((y&3)<<2) + (x&3)) <<1);

    ar =                 (u32)(*((u16*)(bp+offs   )));
    return (ar<<24) | ( ((u32)(*((u16*)(bp+offs+32)))) <<8) | (ar>>8);  // Wii is big-endian
}

/**
 * Set the color value of a pixel to a GRRLIB_texImg.
 * @see GRRLIB_FlushTex
 * @param x Specifies the x-coordinate of the pixel in the texture.
 * @param y Specifies the y-coordinate of the pixel in the texture.
 * @param tex The texture to set the color to.
 * @param color The color of the pixel in RGBA format.
 */
INLINE
void  GRRLIB_SetPixelTotexImg (const int x, const int y,
                               GRRLIB_texImg *tex, const u32 color) {
    register u32  offs;
    register u8*  bp = (u8*)tex->data;

    offs = (((y&(~3))<<2)*tex->w) + ((x&(~3))<<4) + ((((y&3)<<2) + (x&3)) <<1);

    *((u16*)(bp+offs   )) = (u16)((color <<8) | (color >>24));
    *((u16*)(bp+offs+32)) = (u16) (color >>8);
}

/**
 * Reads a pixel directly from the FrontBuffer.
 * @param x The x-coordinate within the FB.
 * @param y The y-coordinate within the FB.
 * @return The color of a pixel in RGBA format.
 */
INLINE
u32 GRRLIB_GetPixelFromFB (int x, int y) {
	u32 regval,val;

	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	val = *(u32*)regval;

    return RGBA(_SHIFTR(val,16,8), _SHIFTR(val,8,8), val&0xff, _SHIFTR(val,24,8));
}

/**
 * Writes a pixel directly from the FrontBuffer.
 * @param x The x-coordinate within the FB.
 * @param y The y-coordinate within the FB.
 * @param pokeColor The color of the pixel in RGBA format.
 */
INLINE
void GRRLIB_SetPixelToFB (int x, int y, u32 pokeColor) {
	u32 regval;

	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	*(u32*)regval = _SHIFTL(A(pokeColor),24,8) | _SHIFTL(R(pokeColor),16,8) | _SHIFTL(G(pokeColor),8,8) | (B(pokeColor)&0xff);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_handle.h
 * Inline functions for manipulating texture handles.
 */

/**
 * Set a texture's X and Y handles.
 * For example, it could be used for the rotation of a texture.
 * @param tex The texture to set the handle on.
 * @param x The x-coordinate of the handle.
 * @param y The y-coordinate of the handle.
 */
INLINE
void  GRRLIB_SetHandle (GRRLIB_texImg *tex, const int x, const int y) {
    if (tex->tiledtex) {
        tex->handlex = -(((int)tex->tilew)/2) + x;
        tex->handley = -(((int)tex->tileh)/2) + y;
    } else {
        tex->handlex = -(((int)tex->w)/2) + x;
        tex->handley = -(((int)tex->h)/2) + y;
    }
}

/**
 * Center a texture's handles.
 * For example, it could be used for the rotation of a texture.
 * @param tex The texture to center.
 * @param enabled
 */
INLINE
void  GRRLIB_SetMidHandle (GRRLIB_texImg *tex, const bool enabled) {
    if (enabled) {
        if (tex->tiledtex) {
            tex->offsetx = (((int)tex->tilew)/2);
            tex->offsety = (((int)tex->tileh)/2);
        } else {
            tex->offsetx = (((int)tex->w)/2);
            tex->offsety = (((int)tex->h)/2);
        }
        GRRLIB_SetHandle(tex, tex->offsetx, tex->offsety);
    } else {
        GRRLIB_SetHandle(tex, 0, 0);
        tex->offsetx = 0;
        tex->offsety = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_fbSimple.h
 * Inline functions for primitive point and line drawing.
 */

/**
 * Clear screen with a specific color.
 * @param color The color to use to fill the screen.
 */
INLINE
void  GRRLIB_FillScreen (const u32 color) {
    GRRLIB_Rectangle(-40, -40, rmode->fbWidth +80, rmode->xfbHeight +80, color, 1);
}

/**
 * Draw a dot.
 * @param x Specifies the x-coordinate of the dot.
 * @param y Specifies the y-coordinate of the dot.
 * @param color The color of the dot in RGBA format.
 * @author Jespa
 */
INLINE
void  GRRLIB_Plot (const f32 x,  const f32 y, const u32 color) {
    GX_Begin(GX_POINTS, GX_VTXFMT0, 1);
        GX_Position3f32(x, y, 0);
        GX_Color1u32(color);
    GX_End();
}

/**
 * Draw a line.
 * @param x1 Starting point for line for the x coordinate.
 * @param y1 Starting point for line for the y coordinate.
 * @param x2 Ending point for line for the x coordinate.
 * @param y2 Ending point for line for the x coordinate.
 * @param color Line color in RGBA format.
 * @author JESPA
 */
INLINE
void  GRRLIB_Line (const f32 x1, const f32 y1,
                   const f32 x2, const f32 y2, const u32 color) {
    GX_Begin(GX_LINES, GX_VTXFMT0, 2);
        GX_Position3f32(x1, y1, 0);
        GX_Color1u32(color);
        GX_Position3f32(x2, y2, 0);
        GX_Color1u32(color);
    GX_End();
}

/**
 * Draw a rectangle.
 * @param x Specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param y Specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param width The width of the rectangle.
 * @param height The height of the rectangle.
 * @param color The color of the rectangle in RGBA format.
 * @param filled Set to true to fill the rectangle.
 */
INLINE
void  GRRLIB_Rectangle (const f32 x,      const f32 y,
                        const f32 width,  const f32 height,
                        const u32 color, const u8 filled) {
    f32 x2 = x + width;
    f32 y2 = y + height;

    if (filled) {
        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position3f32(x, y, 0.0f);
            GX_Color1u32(color);
            GX_Position3f32(x2, y, 0.0f);
            GX_Color1u32(color);
            GX_Position3f32(x2, y2, 0.0f);
            GX_Color1u32(color);
            GX_Position3f32(x, y2, 0.0f);
            GX_Color1u32(color);
        GX_End();
    }
    else {
        GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 5);
            GX_Position3f32(x, y, 0.0f);
            GX_Color1u32(color);
            GX_Position3f32(x2, y, 0.0f);
            GX_Color1u32(color);
            GX_Position3f32(x2, y2, 0.0f);
            GX_Color1u32(color);
            GX_Position3f32(x, y2, 0.0f);
            GX_Color1u32(color);
            GX_Position3f32(x, y, 0.0f);
            GX_Color1u32(color);
        GX_End();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_fbGX.h
 * Inline functions for interfacing directly to the GX Engine.
 */

/**
 * Draws a vector.
 * @param v The vector to draw.
 * @param color The color of the vector in RGBA format.
 * @param n Number of points in the vector.
 * @param fmt Type of primitive.
 */
INLINE
void  GRRLIB_GXEngine (const guVector v[], const u32 color[], const long n,
                       const u8 fmt) {
    int i;

    GX_Begin(fmt, GX_VTXFMT0, n);
    for (i = 0; i < n; i++) {
        GX_Position3f32(v[i].x, v[i].y, v[i].z);
        GX_Color1u32(color[i]);
    }
    GX_End();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_fbComplex.h
 * Inline functions for complex (N-point) shape drawing.
 */

/**
 * Draw an array of points.
 * @param v Array containing the points.
 * @param color The color of the points in RGBA format.
 * @param n Number of points in the vector array.
 */
INLINE
void  GRRLIB_NPlot (const guVector v[], const u32 color[], const long n) {
    GRRLIB_GXEngine(v, color, n, GX_POINTS);
}

/**
 * Draw a polygon.
 * @param v The vector containing the coordinates of the polygon.
 * @param color The color of the filled polygon in RGBA format.
 * @param n Number of points in the vector.
 */
INLINE
void  GRRLIB_NGone (const guVector v[], const u32 color[], const long n) {
    GRRLIB_GXEngine(v, color, n, GX_LINESTRIP);
}

/**
 * Draw a filled polygon.
 * @param v The vector containing the coordinates of the polygon.
 * @param color The color of the filled polygon in RGBA format.
 * @param n Number of points in the vector.
 */
INLINE
void  GRRLIB_NGoneFilled (const guVector v[], const u32 color[], const long n) {
    GRRLIB_GXEngine(v, color, n, GX_TRIANGLEFAN);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_collision.h
 * Inline functions for collision detection.
 */

/**
 * Determine whether the specified point lies within the specified rectangle.
 * @param hotx Specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param hoty Specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param hotw The width of the rectangle.
 * @param hoth The height of the rectangle.
 * @param wpadx Specifies the x-coordinate of the point.
 * @param wpady Specifies the y-coordinate of the point.
 * @return If the specified point lies within the rectangle, the return value is true otherwise it's false.
 */
INLINE
bool  GRRLIB_PtInRect (const int hotx,   const int hoty,
                       const int hotw,   const int hoth,
                       const int wpadx,  const int wpady) {
    return( ((wpadx>=hotx) && (wpadx<=(hotx+hotw))) &&
            ((wpady>=hoty) && (wpady<=(hoty+hoth)))  );
}

/**
 * Determine whether a specified rectangle lies within another rectangle.
 * @param rect1x Specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param rect1y Specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param rect1w Specifies the width of the rectangle.
 * @param rect1h Specifies the height of the rectangle.
 * @param rect2x Specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param rect2y Specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param rect2w Specifies the width of the rectangle.
 * @param rect2h Specifies the height of the rectangle.
 * @return If the specified rectangle lies within the other rectangle, the return value is true otherwise it's false.
 */
INLINE
bool  GRRLIB_RectInRect (const int rect1x, const int rect1y,
                         const int rect1w, const int rect1h,
                         const int rect2x, const int rect2y,
                         const int rect2w, const int rect2h) {
    return ( (rect1x >= rect2x) && (rect1y >= rect2y) &&
             (rect1x+rect1w <= rect2x+rect2w) &&
             (rect1y+rect1h <= rect2y+rect2h) );
}

/**
 * Determine whether a part of a specified rectangle lies on another rectangle.
 * @param rect1x Specifies the x-coordinate of the upper-left corner of the first rectangle.
 * @param rect1y Specifies the y-coordinate of the upper-left corner of the first rectangle.
 * @param rect1w Specifies the width of the first rectangle.
 * @param rect1h Specifies the height of the first rectangle.
 * @param rect2x Specifies the x-coordinate of the upper-left corner of the second rectangle.
 * @param rect2y Specifies the y-coordinate of the upper-left corner of the second rectangle.
 * @param rect2w Specifies the width of the second rectangle.
 * @param rect2h Specifies the height of the second rectangle.
 * @return If the specified rectangle lies on the other rectangle, the return value is true otherwise it's false.
 */
INLINE
bool  GRRLIB_RectOnRect (const int rect1x, const int rect1y,
                         const int rect1w, const int rect1h,
                         const int rect2x, const int rect2y,
                         const int rect2w, const int rect2h) {
    return GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x, rect2y) ||
           GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x+rect2w, rect2y) ||
           GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x+rect2w, rect2y+rect2h) ||
           GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x, rect2y+rect2h);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_clipping.h
 * Inline functions to control clipping.
 */

/**
 * Reset the clipping to normal.
 */
INLINE
void  GRRLIB_ClipReset (void) {
    GX_SetClipMode( GX_CLIP_ENABLE );
    GX_SetScissor( 0, 0, rmode->fbWidth, rmode->efbHeight );
}

/**
 * Clip the drawing area to an rectangle.
 * @param x The x-coordinate of the rectangle.
 * @param y The y-coordinate of the rectangle.
 * @param width The width of the rectangle.
 * @param height The height of the rectangle.
 */
INLINE
void  GRRLIB_ClipDrawing (const int x, const int y,
                          const int width, const int height) {
    GX_SetClipMode( GX_CLIP_ENABLE );
    GX_SetScissor( x, y, width, height );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @file GRRLIB_cExtn.h
 * Inline functions to offer additional C primitives.
 */

/**
 * A helper function for the YCbCr -> RGB conversion.
 * Clamps the given value into a range of 0 - 255 and thus preventing an overflow.
 * @param Value The value to clamp. Using float to increase the precision. This makes a full spectrum (0 - 255) possible.
 * @return Returns a clean, clamped unsigned char.
 */
INLINE
u8  GRRLIB_ClampVar8 (f32 Value) {
    Value = roundf(Value);
    if      (Value < 0)    Value = 0;
    else if (Value > 255)  Value = 255;

    return (u8)Value;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // __GRRLIB_FNINLINE_H__

//==========================================================================================================================================================================================================================================


//==============================================================================
// C++ footer
//==============================================================================
#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif // __GRRLIB_H__
/** @} */ // end of group
/**
 * @mainpage GRRLIB Documentation
 * @image html grrlib_logo.png
 * Welcome to the GRRLIB documentation.
 * A complete list of functions is available from this \ref AllFunc "page".
 *
 * @section Introduction
 * GRRLIB is a C/C++ 2D/3D graphics library for Wii application developers.
 * It is essentially a wrapper which presents a friendly interface to the Nintendo GX core.
 *
 * @section Links
 * Forum: http://grrlib.santo.fr/forum\n
 * Code: http://code.google.com/p/grrlib\n
 * IRC: <a href="irc://irc.efnet.net/grrlib">##GRRLIB</a> on EFnet
 *
 * @section Credits
 * Project Leader : NoNameNo\n
 * Documentation  : Crayon, BlueChip\n
 * Lead Coder     : NoNameNo\n
 * Support Coders : Crayon, Xane, DragonMinded, BlueChip\n
 * Advisors       : RedShade, Jespa\n
 *
 * @section Licence
 * Copyright (c) 2010 The GRRLIB Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @example template/source/main.c
 * This example shows the minimum code required to use GRRLIB.
 * It could be used as a template to start a new project.
 * More elaborate examples can be found inside the \e examples folder.
 */

#ifndef __GRRLIB_PRIVATE_H__
#define __GRRLIB_PRIVATE_H__

#include <ogc/libversion.h>

/**
 * Used for version checking.
 * @param a Major version number.
 * @param b Minor version number.
 * @param c Revision version number.
 */
#define GRRLIB_VERSION(a,b,c) ((a)*65536+(b)*256+(c))

//------------------------------------------------------------------------------
// GRRLIB_ttf.c - FreeType function for GRRLIB
int GRRLIB_InitTTF();
void GRRLIB_ExitTTF();

#endif // __GRRLIB_PRIVATE_H__ 