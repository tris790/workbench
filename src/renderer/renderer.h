/*
 * renderer.h - Renderer abstraction layer for Workbench
 *
 * Backend-agnostic rendering API with pluggable implementations.
 * C99, handmade hero style.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "../core/types.h"

/* ===== Color ===== */

typedef struct {
  u8 r, g, b, a;
} color;

#define COLOR_RGBA(r, g, b, a) ((color){(r), (g), (b), (a)})
#define COLOR_RGB(r, g, b) COLOR_RGBA(r, g, b, 255)

/* Convert color to ARGB u32 for framebuffer */
static inline u32 ColorToARGB(color c) {
  return ((u32)c.a << 24) | ((u32)c.r << 16) | ((u32)c.g << 8) | (u32)c.b;
}

/* ===== Forward Declarations ===== */

typedef struct render_context render_context;
typedef struct renderer_backend renderer_backend;
struct platform_window;

/* Include font.h for font type */
#include "font.h"

/* ===== Renderer Backend Interface ===== */

typedef void (*PFN_BackendInit)(render_context *ctx);
typedef void (*PFN_BackendShutdown)(render_context *ctx);
typedef void (*PFN_BackendBeginFrame)(render_context *ctx);
typedef void (*PFN_BackendEndFrame)(render_context *ctx);
typedef void (*PFN_BackendClear)(render_context *ctx, color c);
typedef void (*PFN_BackendDrawRect)(render_context *ctx, rect r, color c);
typedef void (*PFN_BackendDrawRectRounded)(render_context *ctx, rect r,
                                           f32 radius, color c);
typedef void (*PFN_BackendSetClipRect)(render_context *ctx, rect r);
typedef void (*PFN_BackendDrawText)(render_context *ctx, v2i pos,
                                    const char *text, font *f, color c);
/* Forward declare image struct */
struct image_s;
typedef void (*PFN_BackendDrawImage)(render_context *ctx, rect r,
                                     struct image_s *img, color tint);
typedef void (*PFN_BackendSetWindow)(render_context *ctx,
                                     struct platform_window *window);

struct renderer_backend {
  const char *name;

  PFN_BackendInit init;
  PFN_BackendShutdown shutdown;
  PFN_BackendBeginFrame begin_frame;
  PFN_BackendEndFrame end_frame;
  PFN_BackendClear clear;
  PFN_BackendDrawRect draw_rect;
  PFN_BackendDrawRectRounded draw_rect_rounded;
  PFN_BackendSetClipRect set_clip_rect;
  PFN_BackendDrawText draw_text;
  PFN_BackendDrawImage draw_image;
  PFN_BackendSetWindow set_window;

  b32 presents_frame;

  /* Backend-specific data */
  void *user_data;
};

/* ===== Render Context ===== */

struct render_context {
  renderer_backend *backend;

  /* Framebuffer (for software backend) */
  u32 *pixels;
  i32 width;
  i32 height;
  i32 stride; /* in pixels */

  /* Current clip rect */
  rect clip;

  /* Default font */
  font *default_font;

  struct platform_window *window;
};

/* ===== Backend Registration ===== */

/* Create software rendering backend */
renderer_backend *Render_CreateSoftwareBackend(void);

/* Create OpenGL rendering backend */
renderer_backend *Render_CreateOpenGLBackend(void);

/* Check if OpenGL is available on this system */
b32 Render_OpenGLAvailable(void);

/* Get OpenGL backend state (for platform EGL integration) */
void *Render_GetOpenGLState(renderer_backend *backend);

/* ===== Renderer API ===== */

/* Initialize render context with given backend */
void Render_Init(render_context *ctx, renderer_backend *backend);

/* Shutdown render context */
void Render_Shutdown(render_context *ctx);

/* Set framebuffer (call after resize) */
void Render_SetFramebuffer(render_context *ctx, u32 *pixels, i32 width,
                           i32 height);

/* Set platform window (required for hardware backends) */
void Render_SetWindow(render_context *ctx, struct platform_window *window);

/* Begin frame (call before any draw operations) */
void Render_BeginFrame(render_context *ctx);

/* End frame (call after all draw operations) */
void Render_EndFrame(render_context *ctx);

/* Clear framebuffer with color */
void Render_Clear(render_context *ctx, color c);

/* Set clip rectangle */
void Render_SetClipRect(render_context *ctx, rect r);

/* Reset clip rectangle to full screen */
void Render_ResetClipRect(render_context *ctx);

/* ===== Drawing Primitives ===== */

/* Draw filled rectangle */
void Render_DrawRect(render_context *ctx, rect r, color c);

/* Draw filled rounded rectangle */
void Render_DrawRectRounded(render_context *ctx, rect r, f32 radius, color c);

/* Draw text at position */
void Render_DrawText(render_context *ctx, v2i pos, const char *text, font *f,
                     color c);

/* Draw text with default font */
void Render_DrawTextDefault(render_context *ctx, v2i pos, const char *text,
                            color c);

/* Draw image */
void Render_DrawImage(render_context *ctx, rect r, struct image_s *img,
                      color tint);

#endif /* RENDERER_H */
