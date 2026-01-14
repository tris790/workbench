/*
 * font.h - Font system for Workbench
 *
 * System font loading and text rendering using FreeType.
 * C99, handmade hero style.
 */

#ifndef FONT_H
#define FONT_H

#include "../core/types.h"

/* Forward declaration for color (defined in renderer.h) */
typedef struct {
  u8 r, g, b, a;
} font_color;

/* Font is forward declared - struct defined in font.c */
typedef struct font font;

/* ===== Font System Lifecycle ===== */

/* Initialize font system (call once at startup) */
b32 Font_SystemInit(void);

/* Shutdown font system (call once at exit) */
void Font_SystemShutdown(void);

/* ===== Font Loading ===== */

/* Load font from system by name (e.g., "sans", "monospace") */
font *Font_Load(const char *name, i32 size_pixels);

/* Load font from file path */
font *Font_LoadFromFile(const char *path, i32 size_pixels);
/* Load font from memory */
font *Font_LoadFromMemory(const void *data, usize size, i32 size_pixels);

/* Free font resources */
void Font_Free(font *f);

/* ===== Font Metrics ===== */

/* Get line height in pixels */
i32 Font_GetLineHeight(font *f);

/* Get font ascender (pixels above baseline) */
i32 Font_GetAscender(font *f);

/* Get font descender (pixels below baseline, negative) */
i32 Font_GetDescender(font *f);

/* ===== Text Measurement ===== */

/* Measure text width in pixels */
i32 Font_MeasureWidth(font *f, const char *text);

/* Measure text dimensions */
void Font_MeasureText(font *f, const char *text, i32 *out_width,
                      i32 *out_height);

/* ===== Text Rendering ===== */

/* Render text to framebuffer (called by renderer backend) */
void Font_RenderText(font *f, u32 *pixels, i32 fb_width, i32 fb_height,
                     i32 stride, i32 x, i32 y, const char *text, u8 r, u8 g,
                     u8 b, u8 a, i32 clip_x, i32 clip_y, i32 clip_w,
                     i32 clip_h);

/* ===== Glyph Access ===== */

typedef struct {
  u8 *bitmap;
  i32 width;
  i32 height;
  i32 bearing_x;
  i32 bearing_y;
  i32 advance;
} glyph_bitmap;

/* Get glyph bitmap for a codepoint (for GPU atlas generation)
   Returns true on success, false if character not supported.
   The bitmap field in out_glyph points to internal font cache, do not free. */
b32 Font_GetGlyphBitmap(font *f, u32 codepoint, glyph_bitmap *out_glyph);

#endif /* FONT_H */
