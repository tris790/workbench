#include "font.h"
#include <stdlib.h>
#include <string.h>

/* Stub implementation for Windows without FreeType */

struct font {
  i32 size_pixels;
  i32 line_height;
  i32 ascender;
  i32 descender;
};

b32 Font_SystemInit(void) { return true; }

void Font_SystemShutdown(void) {}

font *Font_Load(const char *name, i32 size_pixels) {
  (void)name;
  font *f = malloc(sizeof(font));
  if (f) {
    f->size_pixels = size_pixels;
    f->line_height = size_pixels + 4;
    f->ascender = size_pixels;
    f->descender = 4;
  }
  return f;
}

font *Font_LoadFromFile(const char *path, i32 size_pixels) {
  (void)path;
  return Font_Load("stub", size_pixels);
}

void Font_Free(font *f) {
  if (f)
    free(f);
}

i32 Font_GetLineHeight(font *f) { return f ? f->line_height : 0; }

i32 Font_GetAscender(font *f) { return f ? f->ascender : 0; }

i32 Font_GetDescender(font *f) { return f ? f->descender : 0; }

i32 Font_MeasureWidth(font *f, const char *text) {
  if (!f || !text)
    return 0;
  return (i32)strlen(text) *
         (f->size_pixels / 2); /* Approximate monospace width */
}

void Font_MeasureText(font *f, const char *text, i32 *out_width,
                      i32 *out_height) {
  if (out_width)
    *out_width = Font_MeasureWidth(f, text);
  if (out_height)
    *out_height = f ? f->line_height : 0;
}

void Font_RenderText(font *f, u32 *pixels, i32 fb_width, i32 fb_height,
                     i32 stride, i32 x, i32 y, const char *text, u8 r, u8 g,
                     u8 b, u8 a, i32 clip_x, i32 clip_y, i32 clip_w,
                     i32 clip_h) {
  if (!f || !pixels || !text)
    return;

  /* Draw placeholder rectangles */
  i32 char_w = f->size_pixels / 2;
  i32 char_h = f->size_pixels;

  i32 pen_x = x;
  i32 pen_y = y + f->ascender - char_h;

  u32 color_val = ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;

  const char *p = text;
  while (*p) {
    if (*p != ' ') {
      /* Draw rect for char */
      for (i32 dy = 0; dy < char_h; dy++) {
        for (i32 dx = 0; dx < char_w; dx++) {
          i32 px = pen_x + dx;
          i32 py = pen_y + dy;

          if (px >= clip_x && px < (clip_x + clip_w) && py >= clip_y &&
              py < (clip_y + clip_h) && px >= 0 && px < fb_width && py >= 0 &&
              py < fb_height) {

            /* Hollow box */
            if (dx == 0 || dx == char_w - 1 || dy == 0 || dy == char_h - 1) {
              pixels[py * stride + px] = color_val;
            }
          }
        }
      }
    }

    pen_x += char_w;
    p++;
  }
}
