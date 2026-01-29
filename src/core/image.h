/*
 * image.h - Image Loading and Management
 *
 * Wraps stb_image for loading and provides an internal representation.
 */

#ifndef IMAGE_H
#define IMAGE_H

#include "types.h"

typedef struct image_s {
  i32 width;
  i32 height;
  i32 channels;   // 4 for RGBA
  u8 *pixels;     // Raw pixel data (RGBA)
  u32 texture_id; // For OpenGL backend (0 if NOT uploaded)
} image;

/* Load image from file path */
image *Image_Load(const char *path);

/* Load image from memory buffer */
image *Image_LoadFromMemory(const u8 *data, usize len);

/* Free image resources */
void Image_Free(image *img);

/* Get pixel color at (x,y) - handling bounds check. Returns 0 if out of bounds.
 * Format: ARGB (Software Renderer native) */
u32 Image_GetPixel(image *img, i32 x, i32 y);

#endif /* IMAGE_H */
