/*
 * font.c - Font system implementation for Workbench
 *
 * Uses FreeType2 for glyph rendering and fontconfig for system fonts.
 * C99, handmade hero style.
 */

#include "font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>

#include <stdlib.h>
#include <string.h>

/* ===== Constants ===== */

#define GLYPH_CACHE_SIZE 256

/* ===== Cached Glyph ===== */

typedef struct {
  u8 *bitmap;
  i32 width;
  i32 height;
  i32 bearing_x;
  i32 bearing_y;
  i32 advance;
  u32 codepoint;
  b32 valid;
} cached_glyph;

/* ===== Font Structure ===== */

struct font {
  FT_Face face;
  i32 size_pixels;
  i32 line_height;
  i32 ascender;
  i32 descender;
  cached_glyph cache[GLYPH_CACHE_SIZE];
};

/* ===== Global State ===== */

static struct {
  FT_Library library;
  b32 initialized;
} g_font_system = {0};

/* ===== Font System Lifecycle ===== */

b32 Font_SystemInit(void) {
  if (g_font_system.initialized)
    return true;

  if (FT_Init_FreeType(&g_font_system.library) != 0) {
    return false;
  }

  g_font_system.initialized = true;
  return true;
}

void Font_SystemShutdown(void) {
  if (!g_font_system.initialized)
    return;

  FT_Done_FreeType(g_font_system.library);
  g_font_system.initialized = false;
}

/* ===== Font Loading ===== */

font *Font_Load(const char *name, i32 size_pixels) {
  if (!g_font_system.initialized) {
    if (!Font_SystemInit())
      return NULL;
  }

  /* Use fontconfig to find system font */
  FcConfig *config = FcInitLoadConfigAndFonts();
  if (!config)
    return NULL;

  FcPattern *pattern = FcNameParse((const FcChar8 *)name);
  if (!pattern) {
    FcConfigDestroy(config);
    return NULL;
  }

  FcConfigSubstitute(config, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result;
  FcPattern *match = FcFontMatch(config, pattern, &result);
  FcPatternDestroy(pattern);

  if (!match) {
    FcConfigDestroy(config);
    return NULL;
  }

  FcChar8 *path = NULL;
  if (FcPatternGetString(match, FC_FILE, 0, &path) != FcResultMatch) {
    FcPatternDestroy(match);
    FcConfigDestroy(config);
    return NULL;
  }

  font *f = Font_LoadFromFile((const char *)path, size_pixels);

  FcPatternDestroy(match);
  FcConfigDestroy(config);

  return f;
}

font *Font_LoadFromFile(const char *path, i32 size_pixels) {
  if (!g_font_system.initialized) {
    if (!Font_SystemInit())
      return NULL;
  }

  font *f = calloc(1, sizeof(font));
  if (!f)
    return NULL;

  if (FT_New_Face(g_font_system.library, path, 0, &f->face) != 0) {
    free(f);
    return NULL;
  }

  if (FT_Set_Pixel_Sizes(f->face, 0, size_pixels) != 0) {
    FT_Done_Face(f->face);
    free(f);
    return NULL;
  }

  f->size_pixels = size_pixels;
  f->ascender = (i32)(f->face->size->metrics.ascender >> 6);
  f->descender = (i32)(f->face->size->metrics.descender >> 6);
  f->line_height = (i32)(f->face->size->metrics.height >> 6);

  return f;
}

void Font_Free(font *f) {
  if (!f)
    return;

  /* Free cached glyph bitmaps */
  for (i32 i = 0; i < GLYPH_CACHE_SIZE; i++) {
    if (f->cache[i].bitmap) {
      free(f->cache[i].bitmap);
    }
  }

  FT_Done_Face(f->face);
  free(f);
}

/* ===== Font Metrics ===== */

i32 Font_GetLineHeight(font *f) { return f ? f->line_height : 0; }

i32 Font_GetAscender(font *f) { return f ? f->ascender : 0; }

i32 Font_GetDescender(font *f) { return f ? f->descender : 0; }

/* ===== Glyph Caching ===== */

static cached_glyph *GetCachedGlyph(font *f, u32 codepoint) {
  if (!f)
    return NULL;

  /* Simple hash for cache lookup */
  u32 index = codepoint % GLYPH_CACHE_SIZE;
  cached_glyph *glyph = &f->cache[index];

  /* Check if already cached */
  if (glyph->valid && glyph->codepoint == codepoint) {
    return glyph;
  }

  /* Free previous glyph if present */
  if (glyph->bitmap) {
    free(glyph->bitmap);
    glyph->bitmap = NULL;
  }

  /* Load glyph */
  FT_UInt glyph_index = FT_Get_Char_Index(f->face, codepoint);
  if (FT_Load_Glyph(f->face, glyph_index, FT_LOAD_DEFAULT) != 0) {
    glyph->valid = false;
    return NULL;
  }

  if (FT_Render_Glyph(f->face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
    glyph->valid = false;
    return NULL;
  }

  FT_GlyphSlot slot = f->face->glyph;
  FT_Bitmap *bmp = &slot->bitmap;

  /* Copy bitmap */
  glyph->width = bmp->width;
  glyph->height = bmp->rows;
  glyph->bearing_x = slot->bitmap_left;
  glyph->bearing_y = slot->bitmap_top;
  glyph->advance = (i32)(slot->advance.x >> 6);
  glyph->codepoint = codepoint;

  if (bmp->width > 0 && bmp->rows > 0) {
    usize size = bmp->width * bmp->rows;
    glyph->bitmap = malloc(size);
    if (glyph->bitmap) {
      /* Copy row by row (bitmap may have padding) */
      for (u32 row = 0; row < bmp->rows; row++) {
        memcpy(glyph->bitmap + row * bmp->width, bmp->buffer + row * bmp->pitch,
               bmp->width);
      }
    }
  }

  glyph->valid = true;
  return glyph;
}

/* ===== Text Measurement ===== */

i32 Font_MeasureWidth(font *f, const char *text) {
  if (!f || !text)
    return 0;

  i32 width = 0;
  const u8 *p = (const u8 *)text;

  while (*p) {
    /* Simple ASCII for now - TODO: UTF-8 */
    u32 codepoint = *p++;

    FT_UInt glyph_index = FT_Get_Char_Index(f->face, codepoint);
    if (FT_Load_Glyph(f->face, glyph_index, FT_LOAD_DEFAULT) == 0) {
      width += (i32)(f->face->glyph->advance.x >> 6);
    }
  }

  return width;
}

void Font_MeasureText(font *f, const char *text, i32 *out_width,
                      i32 *out_height) {
  if (out_width)
    *out_width = Font_MeasureWidth(f, text);
  if (out_height)
    *out_height = f ? f->line_height : 0;
}

/* ===== Text Rendering ===== */

void Font_RenderText(font *f, u32 *pixels, i32 fb_width, i32 fb_height,
                     i32 stride, i32 x, i32 y, const char *text, u8 cr, u8 cg,
                     u8 cb, u8 ca, i32 clip_x, i32 clip_y, i32 clip_w,
                     i32 clip_h) {
  if (!f || !pixels || !text)
    return;

  i32 pen_x = x;
  i32 baseline_y = y + f->ascender;
  const u8 *p = (const u8 *)text;

  /* Clip bounds */
  i32 clip_x0 = clip_x;
  i32 clip_y0 = clip_y;
  i32 clip_x1 = clip_x + clip_w;
  i32 clip_y1 = clip_y + clip_h;

  if (clip_x1 <= 0)
    clip_x1 = fb_width;
  if (clip_y1 <= 0)
    clip_y1 = fb_height;

  u32 text_color = ((u32)cr << 16) | ((u32)cg << 8) | (u32)cb;

  while (*p) {
    /* Simple ASCII for now - TODO: UTF-8 */
    u32 codepoint = *p++;

    cached_glyph *glyph = GetCachedGlyph(f, codepoint);
    if (!glyph || !glyph->bitmap) {
      pen_x += glyph ? glyph->advance : f->size_pixels / 2;
      continue;
    }

    i32 gx = pen_x + glyph->bearing_x;
    i32 gy = baseline_y - glyph->bearing_y;

    /* Render glyph bitmap */
    for (i32 row = 0; row < glyph->height; row++) {
      i32 py = gy + row;
      if (py < clip_y0 || py >= clip_y1)
        continue;

      for (i32 col = 0; col < glyph->width; col++) {
        i32 px = gx + col;
        if (px < clip_x0 || px >= clip_x1)
          continue;

        u8 alpha = glyph->bitmap[row * glyph->width + col];
        if (alpha == 0)
          continue;

        u32 *dst = &pixels[py * stride + px];

        if (alpha == 255 && ca == 255) {
          *dst = 0xFF000000 | text_color;
        } else {
          /* Blend with background */
          u32 da = (*dst >> 24) & 0xFF;
          u32 dr = (*dst >> 16) & 0xFF;
          u32 dg = (*dst >> 8) & 0xFF;
          u32 db = *dst & 0xFF;

          u32 sa = ((u32)alpha * (u32)ca) / 255;
          u32 inv_sa = 255 - sa;

          u32 oa = sa + ((da * inv_sa) >> 8);
          u32 or_ = ((cr * sa) + (dr * inv_sa)) >> 8;
          u32 og = ((cg * sa) + (dg * inv_sa)) >> 8;
          u32 ob = ((cb * sa) + (db * inv_sa)) >> 8;

          *dst = (oa << 24) | (or_ << 16) | (og << 8) | ob;
        }
      }
    }

    pen_x += glyph->advance;
  }
}
