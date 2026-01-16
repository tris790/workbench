/*
 * renderer_software.c - Software rendering backend for Workbench
 *
 * Renders directly to a shared memory framebuffer.
 * C99, handmade hero style.
 */

#include "renderer.h"

#include <math.h>
#include <string.h>

/* ===== Internal Helpers ===== */

static inline i32 ClampI32(i32 value, i32 min, i32 max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static inline f32 ClampF32(f32 value, f32 min, f32 max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

/* Alpha blend: src over dst */
static inline u32 BlendPixel(u32 dst, u32 src) {
  u32 sa = (src >> 24) & 0xFF;
  if (sa == 0)
    return dst;
  if (sa == 255)
    return src;

  u32 da = (dst >> 24) & 0xFF;
  u32 dr = (dst >> 16) & 0xFF;
  u32 dg = (dst >> 8) & 0xFF;
  u32 db = dst & 0xFF;

  u32 sr = (src >> 16) & 0xFF;
  u32 sg = (src >> 8) & 0xFF;
  u32 sb = src & 0xFF;

  u32 inv_sa = 255 - sa;
  u32 oa = sa + ((da * inv_sa) >> 8);
  u32 or = ((sr * sa) + (dr * inv_sa)) >> 8;
  u32 og = ((sg * sa) + (dg * inv_sa)) >> 8;
  u32 ob = ((sb * sa) + (db * inv_sa)) >> 8;

  return (oa << 24) | (or << 16) | (og << 8) | ob;
}

/* ===== Software Backend Implementation ===== */

static void Software_Init(render_context *ctx) {
  (void)ctx;
  /* Nothing to initialize for software backend */
}

static void Software_Shutdown(render_context *ctx) {
  (void)ctx;
  /* Nothing to cleanup for software backend */
}

static void Software_BeginFrame(render_context *ctx) {
  (void)ctx;
  /* Reset clip to full screen */
  ctx->clip = (rect){0, 0, ctx->width, ctx->height};
}

static void Software_EndFrame(render_context *ctx) {
  (void)ctx;
  /* Nothing to do - buffer is already in memory */
}

static void Software_Clear(render_context *ctx, color c) {
  if (!ctx->pixels)
    return;

  u32 pixel = ColorToARGB(c);
  i32 count = ctx->width * ctx->height;

  for (i32 i = 0; i < count; i++) {
    ctx->pixels[i] = pixel;
  }
}

static void Software_DrawRect(render_context *ctx, rect r, color c) {
  if (!ctx->pixels)
    return;

  /* Clip rectangle to clip region */
  i32 x0 = ClampI32(r.x, ctx->clip.x, ctx->clip.x + ctx->clip.w);
  i32 y0 = ClampI32(r.y, ctx->clip.y, ctx->clip.y + ctx->clip.h);
  i32 x1 = ClampI32(r.x + r.w, ctx->clip.x, ctx->clip.x + ctx->clip.w);
  i32 y1 = ClampI32(r.y + r.h, ctx->clip.y, ctx->clip.y + ctx->clip.h);

  if (x0 >= x1 || y0 >= y1)
    return;

  u32 pixel = ColorToARGB(c);

  if (c.a == 255) {
    /* Opaque - direct write */
    for (i32 y = y0; y < y1; y++) {
      u32 *row = ctx->pixels + y * ctx->stride;
      for (i32 x = x0; x < x1; x++) {
        row[x] = pixel;
      }
    }
  } else {
    /* Semi-transparent - blend */
    for (i32 y = y0; y < y1; y++) {
      u32 *row = ctx->pixels + y * ctx->stride;
      for (i32 x = x0; x < x1; x++) {
        row[x] = BlendPixel(row[x], pixel);
      }
    }
  }
}

static void Software_DrawRectRounded(render_context *ctx, rect r, f32 radius,
                                     color c) {
  if (!ctx->pixels)
    return;
  if (radius <= 0) {
    Software_DrawRect(ctx, r, c);
    return;
  }

  /* Clamp radius to half of smallest dimension */
  f32 max_radius = (f32)Min(r.w, r.h) / 2.0f;
  radius = ClampF32(radius, 0, max_radius);

  /* Clip rectangle to clip region */
  i32 x0 = ClampI32(r.x, ctx->clip.x, ctx->clip.x + ctx->clip.w);
  i32 y0 = ClampI32(r.y, ctx->clip.y, ctx->clip.y + ctx->clip.h);
  i32 x1 = ClampI32(r.x + r.w, ctx->clip.x, ctx->clip.x + ctx->clip.w);
  i32 y1 = ClampI32(r.y + r.h, ctx->clip.y, ctx->clip.y + ctx->clip.h);

  if (x0 >= x1 || y0 >= y1)
    return;

  u32 pixel = ColorToARGB(c);
  f32 radius_sq = radius * radius;

  /* Corner centers */
  f32 cx_left = (f32)r.x + radius;
  f32 cx_right = (f32)(r.x + r.w) - radius;
  f32 cy_top = (f32)r.y + radius;
  f32 cy_bottom = (f32)(r.y + r.h) - radius;

  for (i32 y = y0; y < y1; y++) {
    u32 *row = ctx->pixels + y * ctx->stride;
    f32 fy = (f32)y + 0.5f;

    for (i32 x = x0; x < x1; x++) {
      f32 fx = (f32)x + 0.5f;

      /* Check if in corner region */
      f32 dx = 0, dy = 0;

      if (fx < cx_left && fy < cy_top) {
        /* Top-left corner */
        dx = cx_left - fx;
        dy = cy_top - fy;
      } else if (fx > cx_right && fy < cy_top) {
        /* Top-right corner */
        dx = fx - cx_right;
        dy = cy_top - fy;
      } else if (fx < cx_left && fy > cy_bottom) {
        /* Bottom-left corner */
        dx = cx_left - fx;
        dy = fy - cy_bottom;
      } else if (fx > cx_right && fy > cy_bottom) {
        /* Bottom-right corner */
        dx = fx - cx_right;
        dy = fy - cy_bottom;
      }

      if (dx > 0 || dy > 0) {
        f32 dist_sq = dx * dx + dy * dy;
        if (dist_sq > radius_sq)
          continue; /* Outside corner */

        /* Anti-aliasing at edges */
        f32 dist = sqrtf(dist_sq);
        f32 edge = radius - dist;
        if (edge < 1.0f && c.a == 255) {
          u32 alpha = (u32)(edge * 255.0f);
          u32 aa_pixel = (alpha << 24) | (pixel & 0x00FFFFFF);
          row[x] = BlendPixel(row[x], aa_pixel);
          continue;
        }
      }

      if (c.a == 255) {
        row[x] = pixel;
      } else {
        row[x] = BlendPixel(row[x], pixel);
      }
    }
  }
}

static void Software_DrawText(render_context *ctx, v2i pos, const char *text,
                              font *f, color c) {
  if (!ctx->pixels || !text || !f)
    return;

  Font_RenderText(f, ctx->pixels, ctx->width, ctx->height, ctx->stride, pos.x,
                  pos.y, text, c.r, c.g, c.b, c.a, ctx->clip.x, ctx->clip.y,
                  ctx->clip.w, ctx->clip.h);
}

static void Software_SetWindow(render_context *ctx,
                               struct platform_window *window) {
  (void)ctx;
  (void)window;
  /* Software renderer doesn't need the platform window directly,
     it uses pixels provided by the platform. */
}

/* ===== Backend Creation ===== */

static renderer_backend g_software_backend = {
    .name = "Software",
    .init = Software_Init,
    .shutdown = Software_Shutdown,
    .begin_frame = Software_BeginFrame,
    .end_frame = Software_EndFrame,
    .clear = Software_Clear,
    .draw_rect = Software_DrawRect,
    .draw_rect_rounded = Software_DrawRectRounded,
    .set_clip_rect = NULL,
    .draw_text = Software_DrawText,
    .set_window = Software_SetWindow,
    .presents_frame = false,
    .user_data = NULL};

renderer_backend *Render_CreateSoftwareBackend(void) {
  return &g_software_backend;
}

/* ===== Renderer API Implementation ===== */

void Render_Init(render_context *ctx, renderer_backend *backend) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->backend = backend;
  if (backend && backend->init) {
    backend->init(ctx);
  }
}

void Render_Shutdown(render_context *ctx) {
  if (ctx->backend && ctx->backend->shutdown) {
    ctx->backend->shutdown(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
}

void Render_SetFramebuffer(render_context *ctx, u32 *pixels, i32 width,
                           i32 height) {
  ctx->pixels = pixels;
  ctx->width = width;
  ctx->height = height;
  ctx->stride = width;
  ctx->clip = (rect){0, 0, width, height};
}

void Render_SetWindow(render_context *ctx, struct platform_window *window) {
  ctx->window = window;
  if (ctx->backend && ctx->backend->set_window) {
    ctx->backend->set_window(ctx, window);
  }
}

void Render_BeginFrame(render_context *ctx) {
  if (ctx->backend && ctx->backend->begin_frame) {
    ctx->backend->begin_frame(ctx);
  }
}

void Render_EndFrame(render_context *ctx) {
  if (ctx->backend && ctx->backend->end_frame) {
    ctx->backend->end_frame(ctx);
  }
}

void Render_Clear(render_context *ctx, color c) {
  if (ctx->backend && ctx->backend->clear) {
    ctx->backend->clear(ctx, c);
  }
}

void Render_SetClipRect(render_context *ctx, rect r) {
  /* Clamp to screen bounds */
  i32 x0 = ClampI32(r.x, 0, ctx->width);
  i32 y0 = ClampI32(r.y, 0, ctx->height);
  i32 x1 = ClampI32(r.x + r.w, 0, ctx->width);
  i32 y1 = ClampI32(r.y + r.h, 0, ctx->height);
  ctx->clip = (rect){x0, y0, x1 - x0, y1 - y0};

  if (ctx->backend && ctx->backend->set_clip_rect) {
    ctx->backend->set_clip_rect(ctx, ctx->clip);
  }
}

void Render_ResetClipRect(render_context *ctx) {
  ctx->clip = (rect){0, 0, ctx->width, ctx->height};

  if (ctx->backend && ctx->backend->set_clip_rect) {
    ctx->backend->set_clip_rect(ctx, ctx->clip);
  }
}

void Render_DrawRect(render_context *ctx, rect r, color c) {
  if (ctx->backend && ctx->backend->draw_rect) {
    ctx->backend->draw_rect(ctx, r, c);
  }
}

void Render_DrawRectRounded(render_context *ctx, rect r, f32 radius, color c) {
  if (ctx->backend && ctx->backend->draw_rect_rounded) {
    ctx->backend->draw_rect_rounded(ctx, r, radius, c);
  }
}

void Render_DrawText(render_context *ctx, v2i pos, const char *text, font *f,
                     color c) {
  if (ctx->backend && ctx->backend->draw_text) {
    ctx->backend->draw_text(ctx, pos, text, f, c);
  }
}

void Render_DrawTextDefault(render_context *ctx, v2i pos, const char *text,
                            color c) {
  Render_DrawText(ctx, pos, text, ctx->default_font, c);
}
