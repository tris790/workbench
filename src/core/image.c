/*
 * image.c - Image Loading Implementation
 */

#include "image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>

image *Image_Load(const char *path) {
  if (!path)
    return NULL;

  int w, h, n;
  // Force 4 channels (RGBA)
  u8 *data = stbi_load(path, &w, &h, &n, 4);

  if (!data) {
    fprintf(stderr, "Failed to load image: %s\n", path);
    return NULL;
  }

  image *img = (image *)malloc(sizeof(image));
  if (!img) {
    stbi_image_free(data);
    return NULL;
  }

  img->width = w;
  img->height = h;
  img->channels = 4;
  img->pixels = data;
  img->texture_id = 0;

  return img;
}

image *Image_LoadFromMemory(const u8 *data, usize len) {
  if (!data || len == 0)
    return NULL;

  int w, h, n;
  u8 *pixels = stbi_load_from_memory(data, (int)len, &w, &h, &n, 4);

  if (!pixels) {
    return NULL;
  }

  image *img = (image *)malloc(sizeof(image));
  if (!img) {
    stbi_image_free(pixels);
    return NULL;
  }

  img->width = w;
  img->height = h;
  img->channels = 4;
  img->pixels = pixels;
  img->texture_id = 0;

  return img;
}

void Image_Free(image *img) {
  if (!img)
    return;

  if (img->pixels) {
    stbi_image_free(img->pixels);
  }

  free(img);
}

u32 Image_GetPixel(image *img, i32 x, i32 y) {
  if (!img || !img->pixels)
    return 0;
  if (x < 0 || x >= img->width || y < 0 || y >= img->height)
    return 0;

  // Data is RGBA, we want ARGB for software renderer
  u8 *p = img->pixels + (y * img->width + x) * 4;
  u8 r = p[0];
  u8 g = p[1];
  u8 b = p[2];
  u8 a = p[3];

  return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}
