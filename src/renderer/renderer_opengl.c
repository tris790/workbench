/*
 * renderer_opengl.c - OpenGL rendering backend for Workbench
 *
 * GPU-accelerated rendering using OpenGL 3.3 Core Profile.
 * Uses EGL for context creation on Linux/Wayland.
 * C99, handmade hero style.
 */

#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Platform-specific includes ===== */

#ifdef __linux__
#define WB_USE_EGL 1
#include <EGL/egl.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#define GL_GLEXT_PROTOTYPES
#include "../platform/linux/linux_internal.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define WB_USE_WGL 1
#define GL_IMPLEMENTATION
#include "../platform/windows/windows_internal.h"
#include "gl_ext.h"
#include <GL/gl.h>
#include <windows.h>
#endif

/* ===== Constants ===== */

#define GL_MAX_VERTICES 65536
#define GL_MAX_FONT_CACHES 16
#define FONT_ATLAS_SIZE 1024

/* ===== Vertex Structure ===== */

typedef struct {
  f32 x, y;                           /* Position */
  f32 u, v;                           /* Texture coordinates */
  f32 r, g, b, a;                     /* Color */
  f32 rect_x, rect_y, rect_w, rect_h; /* Rectangle bounds for SDF */
  f32 radius;                         /* Corner radius for SDF */
} gl_vertex;

/* ===== Font Glyph Cache ===== */

typedef struct {
  u32 texture_id;
  i32 atlas_width;
  i32 atlas_height;

  /* Glyph metrics in atlas */
  struct {
    b32 valid;
    i32 x, y; /* Position in atlas */
    i32 width, height;
    i32 bearing_x, bearing_y;
    i32 advance;
  } glyphs[256]; /* ASCII only for now */

  font *font_ptr; /* Associated font */
  i32 line_height;
  i32 ascender;
} gl_font_cache;

/* ===== OpenGL Backend State ===== */

typedef struct {
#ifdef WB_USE_EGL
  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLSurface egl_surface;
  struct wl_egl_window *egl_window;
  struct wl_display *wl_display;
  struct wl_surface *wl_surface;
#endif

#ifdef WB_USE_WGL
  HDC hdc;
  HGLRC hglrc;
#endif

  b32 initialized;

  /* Shaders */
  u32 rect_program;
  u32 rounded_rect_program;
  u32 text_program;

  /* VAO/VBO for batched rendering */
  u32 vao;
  u32 vbo;

  /* Vertex batch */
  gl_vertex *vertices;
  u32 vertex_count;
  u32 current_texture;
  u32 current_program;

  /* Font caches */
  gl_font_cache font_caches[GL_MAX_FONT_CACHES];
  i32 font_cache_count;

  /* White pixel texture for solid rects */
  u32 white_texture;

  /* Current viewport size */
  i32 viewport_width;
  i32 viewport_height;

  /* Current clip rect */
  rect clip;

} gl_backend_state;

/* ===== Shader Sources ===== */

static const char *rect_vertex_shader =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "out vec2 v_texcoord;\n"
    "out vec4 v_color;\n"
    "uniform mat4 u_projection;\n"
    "void main() {\n"
    "  gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
    "  v_texcoord = a_texcoord;\n"
    "  v_color = a_color;\n"
    "}\n";

static const char *rect_fragment_shader =
    "#version 330 core\n"
    "in vec2 v_texcoord;\n"
    "in vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "  fragColor = v_color * texture(u_texture, v_texcoord);\n"
    "}\n";

static const char *rounded_rect_vertex_shader =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "layout(location = 3) in vec4 a_rect;\n" /* x, y, w, h */
    "layout(location = 4) in float a_radius;\n"
    "out vec2 v_position;\n"
    "out vec4 v_color;\n"
    "out vec4 v_rect;\n"
    "out float v_radius;\n"
    "uniform mat4 u_projection;\n"
    "void main() {\n"
    "  gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
    "  v_position = a_position;\n"
    "  v_color = a_color;\n"
    "  v_rect = a_rect;\n"
    "  v_radius = a_radius;\n"
    "}\n";

static const char *rounded_rect_fragment_shader =
    "#version 330 core\n"
    "in vec2 v_position;\n"
    "in vec4 v_color;\n"
    "in vec4 v_rect;\n"
    "in float v_radius;\n"
    "out vec4 fragColor;\n"
    "\n"
    "float roundedBoxSDF(vec2 center, vec2 size, float radius) {\n"
    "  vec2 q = abs(center) - size + radius;\n"
    "  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "  vec2 rect_center = v_rect.xy + v_rect.zw * 0.5;\n"
    "  vec2 half_size = v_rect.zw * 0.5;\n"
    "  vec2 p = v_position - rect_center;\n"
    "  float dist = roundedBoxSDF(p, half_size, v_radius);\n"
    "  float alpha = 1.0 - smoothstep(-1.0, 1.0, dist);\n"
    "  fragColor = vec4(v_color.rgb, v_color.a * alpha);\n"
    "}\n";

static const char *text_vertex_shader =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "out vec2 v_texcoord;\n"
    "out vec4 v_color;\n"
    "uniform mat4 u_projection;\n"
    "void main() {\n"
    "  gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
    "  v_texcoord = a_texcoord;\n"
    "  v_color = a_color;\n"
    "}\n";

static const char *text_fragment_shader =
    "#version 330 core\n"
    "in vec2 v_texcoord;\n"
    "in vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "  float alpha = texture(u_texture, v_texcoord).r;\n"
    "  fragColor = vec4(v_color.rgb, v_color.a * alpha);\n"
    "}\n";

/* ===== Stderr Suppression (Linux only, for cleaning up EGL/Driver noise) =====
 */

#ifdef __linux__
static int g_stderr_copy = -1;

static void SuppressStderr(void) {
  int dev_null = open("/dev/null", O_WRONLY);
  if (dev_null != -1) {
    g_stderr_copy = dup(STDERR_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);
  }
}

static void RestoreStderr(void) {
  if (g_stderr_copy != -1) {
    dup2(g_stderr_copy, STDERR_FILENO);
    close(g_stderr_copy);
    g_stderr_copy = -1;
  }
}
#else
static void SuppressStderr(void) {}
static void RestoreStderr(void) {}
#endif

/* ===== Helper Functions ===== */

static u32 CompileShader(GLenum type, const char *source) {
  u32 shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  i32 success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[512];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "Shader compilation error: %s\n", log);
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static u32 CreateShaderProgram(const char *vs_source, const char *fs_source) {
  u32 vs = CompileShader(GL_VERTEX_SHADER, vs_source);
  u32 fs = CompileShader(GL_FRAGMENT_SHADER, fs_source);

  if (!vs || !fs) {
    if (vs)
      glDeleteShader(vs);
    if (fs)
      glDeleteShader(fs);
    return 0;
  }

  u32 program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  glDeleteShader(vs);
  glDeleteShader(fs);

  i32 success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char log[512];
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    fprintf(stderr, "Shader link error: %s\n", log);
    glDeleteProgram(program);
    return 0;
  }

  return program;
}

static void SetProjectionMatrix(u32 program, i32 width, i32 height) {
  /* Orthographic projection: (0,0) top-left, (width, height) bottom-right */
  f32 proj[16] = {2.0f / width, 0, 0, 0, 0, -2.0f / height, 0, 0, 0, 0, -1, 0,
                  -1,           1, 0, 1};

  glUseProgram(program);
  i32 loc = glGetUniformLocation(program, "u_projection");
  if (loc != -1) {
    glUniformMatrix4fv(loc, 1, GL_FALSE, proj);
  }

  /* Also set texture unit 0 as default for all samplers */
  i32 tex_loc = glGetUniformLocation(program, "u_texture");
  if (tex_loc != -1) {
    glUniform1i(tex_loc, 0);
  }
}

/* ===== Font Atlas Generation ===== */

static gl_font_cache *GetFontCache(gl_backend_state *state, font *f) {
  if (!f)
    return NULL;

  /* Check if already cached */
  for (i32 i = 0; i < state->font_cache_count; i++) {
    if (state->font_caches[i].font_ptr == f) {
      return &state->font_caches[i];
    }
  }

  /* Create new cache */
  if (state->font_cache_count >= GL_MAX_FONT_CACHES) {
    fprintf(stderr, "OpenGL: Too many font caches\n");
    return NULL;
  }

  gl_font_cache *cache = &state->font_caches[state->font_cache_count];
  memset(cache, 0, sizeof(*cache));
  cache->font_ptr = f;

  /* Get font metrics */
  cache->line_height = Font_GetLineHeight(f);
  cache->ascender = Font_GetAscender(f);

  /* Create atlas texture */
  cache->atlas_width = FONT_ATLAS_SIZE;
  cache->atlas_height = FONT_ATLAS_SIZE;

  u8 *atlas_data = calloc(1, FONT_ATLAS_SIZE * FONT_ATLAS_SIZE);
  if (!atlas_data)
    return NULL;

  /* Pack ASCII glyphs into atlas */
  i32 pen_x = 1;
  i32 pen_y = 1;
  i32 row_height = 0;

  for (u32 c = 32; c < 128; c++) {
    glyph_bitmap gb;
    if (!Font_GetGlyphBitmap(f, c, &gb))
      continue;

    /* Check if glyph fits on current row */
    if (pen_x + gb.width + 1 > FONT_ATLAS_SIZE) {
      pen_x = 1;
      pen_y += row_height + 1;
      row_height = 0;
    }

    /* Check if fits vertically */
    if (pen_y + gb.height + 1 > FONT_ATLAS_SIZE) {
      fprintf(stderr, "OpenGL: Font atlas too small\n");
      break;
    }

    /* Copy glyph bitmap to atlas */
    for (i32 row = 0; row < gb.height; row++) {
      for (i32 col = 0; col < gb.width; col++) {
        i32 atlas_idx = (pen_y + row) * FONT_ATLAS_SIZE + (pen_x + col);
        atlas_data[atlas_idx] = gb.bitmap[row * gb.width + col];
      }
    }

    /* Store glyph metrics */
    cache->glyphs[c].valid = true;
    cache->glyphs[c].x = pen_x;
    cache->glyphs[c].y = pen_y;
    cache->glyphs[c].width = gb.width;
    cache->glyphs[c].height = gb.height;
    cache->glyphs[c].bearing_x = gb.bearing_x;
    cache->glyphs[c].bearing_y = gb.bearing_y;
    cache->glyphs[c].advance = gb.advance;

    pen_x += gb.width + 1;
    if (gb.height > row_height)
      row_height = gb.height;
  }

  /* Upload atlas to GPU */
  glGenTextures(1, &cache->texture_id);
  glBindTexture(GL_TEXTURE_2D, cache->texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FONT_ATLAS_SIZE, FONT_ATLAS_SIZE, 0,
               GL_RED, GL_UNSIGNED_BYTE, atlas_data);

  free(atlas_data);

  state->font_cache_count++;
  return cache;
}

/* ===== Batch Rendering ===== */

static void FlushBatch(gl_backend_state *state) {
  if (state->vertex_count == 0)
    return;

  /* Bind VAO to ensure correct state */
  glBindVertexArray(state->vao);

  /* Upload vertices */
  glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, state->vertex_count * sizeof(gl_vertex),
                  state->vertices);

  /* Draw */
  glDrawArrays(GL_TRIANGLES, 0, state->vertex_count);

  state->vertex_count = 0;
}

static void EnsureBatchCapacity(gl_backend_state *state, u32 count) {
  if (state->vertex_count + count > GL_MAX_VERTICES) {
    FlushBatch(state);
  }
}

static void SetBatchState(gl_backend_state *state, u32 program, u32 texture) {
  if (state->current_program != program || state->current_texture != texture) {
    FlushBatch(state);
    state->current_program = program;
    state->current_texture = texture;

    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D, texture);
  }
}

/* ===== Backend Implementation ===== */

static void GL_Init(render_context *ctx) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;

  if (state->initialized)
    return;

#ifdef WB_USE_EGL
  /* Get Wayland display from platform - we need to access it externally */
  /* For now, we'll initialize EGL when we have surface info */
  /* This will be called from Platform layer */
#endif

  /* Allocate vertex buffer */
  state->vertices = malloc(GL_MAX_VERTICES * sizeof(gl_vertex));
  if (!state->vertices) {
    fprintf(stderr, "OpenGL: Failed to allocate vertex buffer\n");
    return;
  }

  state->initialized = true;
}

static void GL_Shutdown(render_context *ctx) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;

  if (!state->initialized)
    return;

  /* Delete font textures */
  for (i32 i = 0; i < state->font_cache_count; i++) {
    if (state->font_caches[i].texture_id) {
      glDeleteTextures(1, &state->font_caches[i].texture_id);
    }
  }

  /* Delete white texture */
  if (state->white_texture) {
    glDeleteTextures(1, &state->white_texture);
  }

  /* Delete shaders */
  if (state->rect_program)
    glDeleteProgram(state->rect_program);
  if (state->rounded_rect_program)
    glDeleteProgram(state->rounded_rect_program);
  if (state->text_program)
    glDeleteProgram(state->text_program);

  /* Delete VAO/VBO */
  if (state->vbo)
    glDeleteBuffers(1, &state->vbo);
  if (state->vao)
    glDeleteVertexArrays(1, &state->vao);

  /* Free vertex buffer */
  free(state->vertices);
  state->vertices = NULL;

#ifdef WB_USE_EGL
  if (state->egl_context) {
    eglDestroyContext(state->egl_display, state->egl_context);
  }
  if (state->egl_surface) {
    eglDestroySurface(state->egl_display, state->egl_surface);
  }
  if (state->egl_window) {
    wl_egl_window_destroy(state->egl_window);
  }
  if (state->egl_display) {
    eglTerminate(state->egl_display);
  }
#endif

  state->initialized = false;
}

static void GL_BeginFrame(render_context *ctx) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;

  /* Update viewport if size changed */
  if (ctx->width != state->viewport_width ||
      ctx->height != state->viewport_height) {
    state->viewport_width = ctx->width;
    state->viewport_height = ctx->height;
    glViewport(0, 0, ctx->width, ctx->height);

#ifdef WB_USE_EGL
    /* MUST resize the Wayland EGL window or nothing will show or be distorted
     */
    if (state->egl_window) {
      wl_egl_window_resize(state->egl_window, ctx->width, ctx->height, 0, 0);
    }
#endif

    /* Update projection matrices for all shaders */
    SetProjectionMatrix(state->rect_program, ctx->width, ctx->height);
    SetProjectionMatrix(state->rounded_rect_program, ctx->width, ctx->height);
    SetProjectionMatrix(state->text_program, ctx->width, ctx->height);
  }

  /* Reset clip to full screen */
  ctx->clip = (rect){0, 0, ctx->width, ctx->height};
  state->clip = ctx->clip;
  glScissor(0, 0, ctx->width, ctx->height);

  /* Reset batch state */
  state->vertex_count = 0;
  state->current_program = 0;
  state->current_texture = 0;
}

static void GL_EndFrame(render_context *ctx) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;

  /* Flush any remaining vertices */
  FlushBatch(state);

#ifdef WB_USE_EGL
  /* Swap buffers */
  if (state->egl_surface) {
    eglSwapBuffers(state->egl_display, state->egl_surface);
  }
#endif

#ifdef WB_USE_WGL
  SwapBuffers(state->hdc);
#endif
}

static void GL_Clear(render_context *ctx, color c) {
  (void)ctx;
  glClearColor(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

static void GL_DrawRect(render_context *ctx, rect r, color c) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;

  SetBatchState(state, state->rect_program, state->white_texture);
  EnsureBatchCapacity(state, 6);

  f32 x0 = (f32)r.x;
  f32 y0 = (f32)r.y;
  f32 x1 = (f32)(r.x + r.w);
  f32 y1 = (f32)(r.y + r.h);

  f32 cr = c.r / 255.0f;
  f32 cg = c.g / 255.0f;
  f32 cb = c.b / 255.0f;
  f32 ca = c.a / 255.0f;

  gl_vertex *v = &state->vertices[state->vertex_count];

  /* Triangle 1 */
  v[0] = (gl_vertex){x0, y0, 0, 0, cr, cg, cb, ca, 0, 0, 0, 0, 0};
  v[1] = (gl_vertex){x1, y0, 1, 0, cr, cg, cb, ca, 0, 0, 0, 0, 0};
  v[2] = (gl_vertex){x1, y1, 1, 1, cr, cg, cb, ca, 0, 0, 0, 0, 0};

  /* Triangle 2 */
  v[3] = (gl_vertex){x0, y0, 0, 0, cr, cg, cb, ca, 0, 0, 0, 0, 0};
  v[4] = (gl_vertex){x1, y1, 1, 1, cr, cg, cb, ca, 0, 0, 0, 0, 0};
  v[5] = (gl_vertex){x0, y1, 0, 1, cr, cg, cb, ca, 0, 0, 0, 0, 0};

  state->vertex_count += 6;
}

static void GL_DrawRectRounded(render_context *ctx, rect r, f32 radius,
                               color c) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;

  if (radius <= 0) {
    GL_DrawRect(ctx, r, c);
    return;
  }

  /* Clamp radius */
  f32 max_radius = (f32)Min(r.w, r.h) / 2.0f;
  if (radius > max_radius)
    radius = max_radius;

  /* Flush current batch since we're switching programs */
  FlushBatch(state);

  SetBatchState(state, state->rounded_rect_program, state->white_texture);
  EnsureBatchCapacity(state, 6);

  f32 x0 = (f32)r.x;
  f32 y0 = (f32)r.y;
  f32 x1 = (f32)(r.x + r.w);
  f32 y1 = (f32)(r.y + r.h);

  f32 cr = c.r / 255.0f;
  f32 cg = c.g / 255.0f;
  f32 cb = c.b / 255.0f;
  f32 ca = c.a / 255.0f;

  gl_vertex *v = &state->vertices[state->vertex_count];

  /* All vertices share the same rect and radius for SDF calculation */
  f32 rx = (f32)r.x;
  f32 ry = (f32)r.y;
  f32 rw = (f32)r.w;
  f32 rh = (f32)r.h;

  /* Triangle 1 */
  v[0] = (gl_vertex){x0, y0, 0, 0, cr, cg, cb, ca, rx, ry, rw, rh, radius};
  v[1] = (gl_vertex){x1, y0, 1, 0, cr, cg, cb, ca, rx, ry, rw, rh, radius};
  v[2] = (gl_vertex){x1, y1, 1, 1, cr, cg, cb, ca, rx, ry, rw, rh, radius};

  /* Triangle 2 */
  v[3] = (gl_vertex){x0, y0, 0, 0, cr, cg, cb, ca, rx, ry, rw, rh, radius};
  v[4] = (gl_vertex){x1, y1, 1, 1, cr, cg, cb, ca, rx, ry, rw, rh, radius};
  v[5] = (gl_vertex){x0, y1, 0, 1, cr, cg, cb, ca, rx, ry, rw, rh, radius};

  state->vertex_count += 6;
}

static void GL_DrawText(render_context *ctx, v2i pos, const char *text, font *f,
                        color c) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;

  if (!text || !f)
    return;

  gl_font_cache *cache = GetFontCache(state, f);
  if (!cache)
    return;

  SetBatchState(state, state->text_program, cache->texture_id);

  f32 pen_x = (f32)pos.x;
  f32 baseline_y = (f32)pos.y + (f32)cache->ascender;

  f32 cr = c.r / 255.0f;
  f32 cg = c.g / 255.0f;
  f32 cb = c.b / 255.0f;
  f32 ca = c.a / 255.0f;

  f32 inv_atlas_w = 1.0f / (f32)cache->atlas_width;
  f32 inv_atlas_h = 1.0f / (f32)cache->atlas_height;

  const u8 *p = (const u8 *)text;
  while (*p) {
    u32 ch = *p++;
    if (ch >= 256 || !cache->glyphs[ch].valid) {
      pen_x += cache->glyphs[' '].advance;
      continue;
    }

    EnsureBatchCapacity(state, 6);

    i32 gx = cache->glyphs[ch].x;
    i32 gy = cache->glyphs[ch].y;
    i32 gw = cache->glyphs[ch].width;
    i32 gh = cache->glyphs[ch].height;
    i32 bearing_x = cache->glyphs[ch].bearing_x;
    i32 bearing_y = cache->glyphs[ch].bearing_y;
    i32 advance = cache->glyphs[ch].advance;

    f32 x0 = pen_x + bearing_x;
    f32 y0 = baseline_y - bearing_y;
    f32 x1 = x0 + gw;
    f32 y1 = y0 + gh;

    f32 u0 = gx * inv_atlas_w;
    f32 v0 = gy * inv_atlas_h;
    f32 u1 = (gx + gw) * inv_atlas_w;
    f32 v1 = (gy + gh) * inv_atlas_h;

    gl_vertex *v = &state->vertices[state->vertex_count];

    /* Triangle 1 */
    v[0] = (gl_vertex){x0, y0, u0, v0, cr, cg, cb, ca, 0, 0, 0, 0, 0};
    v[1] = (gl_vertex){x1, y0, u1, v0, cr, cg, cb, ca, 0, 0, 0, 0, 0};
    v[2] = (gl_vertex){x1, y1, u1, v1, cr, cg, cb, ca, 0, 0, 0, 0, 0};

    /* Triangle 2 */
    v[3] = (gl_vertex){x0, y0, u0, v0, cr, cg, cb, ca, 0, 0, 0, 0, 0};
    v[4] = (gl_vertex){x1, y1, u1, v1, cr, cg, cb, ca, 0, 0, 0, 0, 0};
    v[5] = (gl_vertex){x0, y1, u0, v1, cr, cg, cb, ca, 0, 0, 0, 0, 0};

    state->vertex_count += 6;
    pen_x += advance;
  }
}

/* ===== EGL Initialization (call from platform after surface creation) ===== */

#ifdef WB_USE_EGL
b32 GL_InitEGL(gl_backend_state *state, struct wl_display *display,
               struct wl_surface *surface, i32 width, i32 height) {
  state->wl_display = display;
  state->wl_surface = surface;

  /* Initialize EGL */
  SuppressStderr();
  state->egl_display = eglGetDisplay((EGLNativeDisplayType)display);
  if (state->egl_display == EGL_NO_DISPLAY) {
    RestoreStderr();
    fprintf(stderr, "OpenGL: Failed to get EGL display\n");
    return false;
  }

  EGLint major, minor;
  if (!eglInitialize(state->egl_display, &major, &minor)) {
    RestoreStderr();
    fprintf(stderr, "OpenGL: Failed to initialize EGL\n");
    return false;
  }
  RestoreStderr();

  /* Choose config */
  EGLint config_attribs[] = {EGL_SURFACE_TYPE,
                             EGL_WINDOW_BIT,
                             EGL_RED_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_ALPHA_SIZE,
                             8,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_BIT,
                             EGL_NONE};

  EGLConfig config;
  EGLint num_configs;
  if (!eglChooseConfig(state->egl_display, config_attribs, &config, 1,
                       &num_configs) ||
      num_configs == 0) {
    fprintf(stderr, "OpenGL: Failed to choose EGL config\n");
    return false;
  }

  /* Create EGL window surface */
  state->egl_window = wl_egl_window_create(surface, width, height);
  if (!state->egl_window) {
    fprintf(stderr, "OpenGL: Failed to create Wayland EGL window\n");
    return false;
  }

  state->egl_surface = eglCreateWindowSurface(
      state->egl_display, config, (EGLNativeWindowType)state->egl_window, NULL);
  if (state->egl_surface == EGL_NO_SURFACE) {
    fprintf(stderr, "OpenGL: Failed to create EGL surface\n");
    return false;
  }

  /* Create OpenGL context */
  eglBindAPI(EGL_OPENGL_API);

  EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION,
                              3,
                              EGL_CONTEXT_MINOR_VERSION,
                              3,
                              EGL_CONTEXT_OPENGL_PROFILE_MASK,
                              EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                              EGL_NONE};

  state->egl_context = eglCreateContext(state->egl_display, config,
                                        EGL_NO_CONTEXT, context_attribs);
  if (state->egl_context == EGL_NO_CONTEXT) {
    fprintf(stderr, "OpenGL: Failed to create EGL context\n");
    return false;
  }

  if (!eglMakeCurrent(state->egl_display, state->egl_surface,
                      state->egl_surface, state->egl_context)) {
    fprintf(stderr, "OpenGL: Failed to make context current\n");
    return false;
  }

  /* VSync */
  eglSwapInterval(state->egl_display, 1);

  /* Initialize OpenGL state */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  /* Create shaders */
  state->rect_program =
      CreateShaderProgram(rect_vertex_shader, rect_fragment_shader);
  state->rounded_rect_program = CreateShaderProgram(
      rounded_rect_vertex_shader, rounded_rect_fragment_shader);
  state->text_program =
      CreateShaderProgram(text_vertex_shader, text_fragment_shader);

  if (!state->rect_program || !state->rounded_rect_program ||
      !state->text_program) {
    fprintf(stderr, "OpenGL: Failed to create shaders\n");
    return false;
  }

  /* Create VAO */
  glGenVertexArrays(1, &state->vao);
  glBindVertexArray(state->vao);

  /* Create VBO */
  glGenBuffers(1, &state->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
  glBufferData(GL_ARRAY_BUFFER, GL_MAX_VERTICES * sizeof(gl_vertex), NULL,
               GL_DYNAMIC_DRAW);

  /* Set up vertex attributes */
  /* Position */
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, x));
  glEnableVertexAttribArray(0);

  /* Texcoord */
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, u));
  glEnableVertexAttribArray(1);

  /* Color */
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, r));
  glEnableVertexAttribArray(2);

  /* Rect bounds (for rounded rect SDF) */
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, rect_x));
  glEnableVertexAttribArray(3);

  /* Radius */
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, radius));
  glEnableVertexAttribArray(4);

  /* Create white texture for solid rectangles */
  u8 white_pixel[4] = {255, 255, 255, 255};
  glGenTextures(1, &state->white_texture);
  glBindTexture(GL_TEXTURE_2D, state->white_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               white_pixel);

  /* Force a projection matrix update on first frame */
  state->viewport_width = -1;
  state->viewport_height = -1;

  return true;
}

void GL_ResizeEGL(gl_backend_state *state, i32 width, i32 height) {
  if (state->egl_window) {
    wl_egl_window_resize(state->egl_window, width, height, 0, 0);
  }
}
#endif /* WB_USE_EGL */

#ifdef WB_USE_WGL
static void LoadGLExtensions(void) {
  glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
  glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
  glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
  glBufferSubData =
      (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubData");
  glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
  glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
  glCompileShader =
      (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
  glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
  glGetShaderInfoLog =
      (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
  glCreateProgram =
      (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
  glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
  glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
  glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
  glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
  glGetProgramInfoLog =
      (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
  glGenVertexArrays =
      (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
  glBindVertexArray =
      (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
  glVertexAttribPointer =
      (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
  glEnableVertexAttribArray =
      (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress(
          "glEnableVertexAttribArray");
  glGetUniformLocation =
      (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
  glUniformMatrix4fv =
      (PFNGLUNIFORMMATRIX4FVPROC)wglGetProcAddress("glUniformMatrix4fv");
  glActiveTexture =
      (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture");
  glDeleteBuffers =
      (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
  glDeleteVertexArrays =
      (PFNGLDELETEVERTEXARRAYSPROC)wglGetProcAddress("glDeleteVertexArrays");
  glDeleteProgram =
      (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");
  glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
  glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
  glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
  glUniform2f = (PFNGLUNIFORM2FPROC)wglGetProcAddress("glUniform2f");
  glUniform3f = (PFNGLUNIFORM3FPROC)wglGetProcAddress("glUniform3f");
  glUniform4f = (PFNGLUNIFORM4FPROC)wglGetProcAddress("glUniform4f");

  wglCreateContextAttribsARB =
      (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress(
          "wglCreateContextAttribsARB");
  wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress(
      "wglChoosePixelFormatARB");
  wglSwapIntervalEXT =
      (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
}

b32 GL_InitWGL(gl_backend_state *state, HDC hdc, i32 width, i32 height) {
  state->hdc = hdc;

  PIXELFORMATDESCRIPTOR pfd = {0};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;

  int format = ChoosePixelFormat(hdc, &pfd);
  SetPixelFormat(hdc, format, &pfd);

  HGLRC temp_context = wglCreateContext(hdc);
  wglMakeCurrent(hdc, temp_context);

  LoadGLExtensions();

  if (wglCreateContextAttribsARB) {
    int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                     3,
                     WGL_CONTEXT_MINOR_VERSION_ARB,
                     3,
                     WGL_CONTEXT_PROFILE_MASK_ARB,
                     WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                     0};
    state->hglrc = wglCreateContextAttribsARB(hdc, 0, attribs);
    if (state->hglrc) {
      wglMakeCurrent(NULL, NULL);
      wglDeleteContext(temp_context);
      wglMakeCurrent(hdc, state->hglrc);
    } else {
      state->hglrc = temp_context;
    }
  } else {
    state->hglrc = temp_context;
  }

  if (wglSwapIntervalEXT) {
    wglSwapIntervalEXT(1);
  }

  /* Initialize OpenGL state */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(0x0C11); /* GL_SCISSOR_TEST */
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  /* Create shaders */
  state->rect_program =
      CreateShaderProgram(rect_vertex_shader, rect_fragment_shader);
  state->rounded_rect_program = CreateShaderProgram(
      rounded_rect_vertex_shader, rounded_rect_fragment_shader);
  state->text_program =
      CreateShaderProgram(text_vertex_shader, text_fragment_shader);

  /* Create VAO */
  glGenVertexArrays(1, &state->vao);
  glBindVertexArray(state->vao);

  /* Create VBO */
  glGenBuffers(1, &state->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
  glBufferData(GL_ARRAY_BUFFER, GL_MAX_VERTICES * sizeof(gl_vertex), NULL,
               GL_DYNAMIC_DRAW);

  /* Set up vertex attributes (Same as EGL) */
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, x));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, u));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, r));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, rect_x));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (void *)offsetof(gl_vertex, radius));
  glEnableVertexAttribArray(4);

  /* Create white texture */
  u8 white_pixel[4] = {255, 255, 255, 255};
  glGenTextures(1, &state->white_texture);
  glBindTexture(GL_TEXTURE_2D, state->white_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               white_pixel);

  /* Force a projection matrix update on first frame */
  state->viewport_width = -1;
  state->viewport_height = -1;

  return true;
}
#endif /* WB_USE_WGL */

static void GL_SetWindow(render_context *ctx, struct platform_window *window) {
  gl_backend_state *state = (gl_backend_state *)ctx->backend->user_data;
  if (!window)
    return;

#ifdef WB_USE_EGL
  /* Bridge to platform-specific types */
  GL_InitEGL(state, g_platform.display, window->surface, window->width,
             window->height);
#endif

#ifdef WB_USE_WGL
  GL_InitWGL(state, window->hdc, window->width, window->height);
#endif
}

/* ===== Backend Creation ===== */

static gl_backend_state g_gl_state = {0};

static renderer_backend g_opengl_backend = {.name = "OpenGL",
                                            .init = GL_Init,
                                            .shutdown = GL_Shutdown,
                                            .begin_frame = GL_BeginFrame,
                                            .end_frame = GL_EndFrame,
                                            .clear = GL_Clear,
                                            .draw_rect = GL_DrawRect,
                                            .draw_rect_rounded =
                                                GL_DrawRectRounded,
                                            .draw_text = GL_DrawText,
                                            .set_window = GL_SetWindow,
                                            .presents_frame = true,
                                            .user_data = &g_gl_state};

renderer_backend *Render_CreateOpenGLBackend(void) { return &g_opengl_backend; }

/* ===== OpenGL Availability Check ===== */

b32 Render_OpenGLAvailable(void) {
#ifdef WB_USE_EGL
  /* Check if EGL is available */
  SuppressStderr();
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    RestoreStderr();
    return false;
  }

  EGLint major, minor;
  if (!eglInitialize(display, &major, &minor)) {
    RestoreStderr();
    return false;
  }
  eglTerminate(display);
  RestoreStderr();
  return true;
#elif defined(WB_USE_WGL)
  return true;
#else
  return false;
#endif
}

/* ===== Get Backend State (for platform integration) ===== */

void *Render_GetOpenGLState(renderer_backend *backend) {
  if (backend && backend->user_data) {
    return backend->user_data;
  }
  return NULL;
}
