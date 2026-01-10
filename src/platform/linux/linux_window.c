/*
 * linux_window.c - Linux/Wayland window management
 */

#include "linux_internal.h"
#include <time.h>

platform_window *g_current_window = NULL;

/* ===== Internal Helpers ===== */

void DestroyShmBuffers(platform_window *window) {
  for (i32 i = 0; i < 2; i++) {
    if (window->shm_data[i] && window->shm_data[i] != MAP_FAILED) {
      munmap(window->shm_data[i], window->shm_size);
      window->shm_data[i] = NULL;
    }
    if (window->buffers[i]) {
      wl_buffer_destroy(window->buffers[i]);
      window->buffers[i] = NULL;
    }
  }
}

i32 CreateShmBuffer(platform_window *window, i32 index) {
  usize stride = window->width * 4; /* ARGB */
  usize size = stride * window->height;
  window->shm_size = size;

  char name[64];
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  snprintf(name, sizeof(name), "/wb-shm-%d-%d-%ld", getpid(), index, ts.tv_nsec);

  i32 fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    return -1;
  shm_unlink(name);

  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }

  window->shm_data[index] =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (window->shm_data[index] == MAP_FAILED) {
    close(fd);
    return -1;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(g_platform.shm, fd, size);
  window->buffers[index] = wl_shm_pool_create_buffer(
      pool, 0, window->width, window->height, stride, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);

  return 0;
}

/* ===== XDG Surface Callbacks ===== */

static void XdgSurfaceConfigure(void *data, struct xdg_surface *xdg_surface,
                                u32 serial) {
  platform_window *window = (platform_window *)data;
  xdg_surface_ack_configure(xdg_surface, serial);
  window->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = XdgSurfaceConfigure,
};

static void XdgToplevelConfigure(void *data, struct xdg_toplevel *toplevel,
                                 i32 width, i32 height,
                                 struct wl_array *states) {
  (void)toplevel;
  (void)states;
  platform_window *window = (platform_window *)data;

  if (width > 0 && height > 0) {
    if (window->width != width || window->height != height) {
      
      /* Resize buffers */
      DestroyShmBuffers(window);
      
      window->width = width;
      window->height = height;
      
      CreateShmBuffer(window, 0);
      CreateShmBuffer(window, 1);

      platform_event event = {0};
      event.type = EVENT_WINDOW_RESIZE;
      event.data.resize.width = width;
      event.data.resize.height = height;
      PushEvent(window, &event);
    }
  }
}

static void XdgToplevelClose(void *data, struct xdg_toplevel *toplevel) {
  (void)toplevel;
  platform_window *window = (platform_window *)data;
  window->should_close = true;

  platform_event event = {0};
  event.type = EVENT_QUIT;
  PushEvent(window, &event);
}

static void XdgToplevelConfigureBounds(void *data,
                                       struct xdg_toplevel *toplevel, i32 width,
                                       i32 height) {
  (void)data; (void)toplevel; (void)width; (void)height;
}

static void XdgToplevelWmCapabilities(void *data, struct xdg_toplevel *toplevel,
                                      struct wl_array *capabilities) {
  (void)data; (void)toplevel; (void)capabilities;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = XdgToplevelConfigure,
    .close = XdgToplevelClose,
    .configure_bounds = XdgToplevelConfigureBounds,
    .wm_capabilities = XdgToplevelWmCapabilities,
};

/* ===== Window API ===== */

extern const struct wl_keyboard_listener keyboard_listener;
extern const struct wl_pointer_listener pointer_listener;

platform_window *Platform_CreateWindow(window_config *config) {
  if (!g_platform.initialized) {
    if (!Platform_Init())
      return NULL;
  }

  platform_window *window = calloc(1, sizeof(platform_window));
  if (!window)
    return NULL;

  window->width = config->width;
  window->height = config->height;
  window->resizable = config->resizable;

  window->surface = wl_compositor_create_surface(g_platform.compositor);
  if (!window->surface) {
    free(window);
    return NULL;
  }

  window->xdg_surface =
      xdg_wm_base_get_xdg_surface(g_platform.xdg_wm_base, window->surface);
  xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);

  window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
  xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener,
                            window);

  xdg_toplevel_set_title(window->xdg_toplevel, config->title);
  xdg_toplevel_set_app_id(window->xdg_toplevel, "workbench");

  if (config->resizable) {
    xdg_toplevel_set_min_size(window->xdg_toplevel, 400, 300);
  }

  if (config->maximized) {
    xdg_toplevel_set_maximized(window->xdg_toplevel);
  }

  /* Request server-side decorations if available */
  if (g_platform.decoration_manager) {
    window->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
        g_platform.decoration_manager, window->xdg_toplevel);
    if (window->decoration) {
      zxdg_toplevel_decoration_v1_set_mode(
          window->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
  }

  wl_surface_commit(window->surface);
  wl_display_roundtrip(g_platform.display);

  /* Create double buffers */
  if (CreateShmBuffer(window, 0) < 0 || CreateShmBuffer(window, 1) < 0) {
    Platform_DestroyWindow(window);
    return NULL;
  }
  window->current_buffer = 0;

  /* Clear both buffers to dark background */
  for (i32 b = 0; b < 2; b++) {
    u32 *pixels = (u32 *)window->shm_data[b];
    for (i32 i = 0; i < window->width * window->height; i++) {
        /* Check if shm_data is valid before writing */
        if (pixels) {
            pixels[i] = 0xFF1E1E2E; /* Dark theme background */
        }
    }
  }

  wl_surface_attach(window->surface, window->buffers[0], 0, 0);
  wl_surface_commit(window->surface);

  g_current_window = window;

  /* Attach input listeners if already available */
  if (g_platform.keyboard) {
    wl_keyboard_add_listener(g_platform.keyboard, &keyboard_listener, window);
  }
  if (g_platform.pointer) {
    wl_pointer_add_listener(g_platform.pointer, &pointer_listener, window);
  }

  return window;
}

void Platform_DestroyWindow(platform_window *window) {
  if (!window)
    return;

  DestroyShmBuffers(window);
  if (window->decoration)
    zxdg_toplevel_decoration_v1_destroy(window->decoration);
  if (window->xdg_toplevel)
    xdg_toplevel_destroy(window->xdg_toplevel);
  if (window->xdg_surface)
    xdg_surface_destroy(window->xdg_surface);
  if (window->surface)
    wl_surface_destroy(window->surface);

  if (g_current_window == window) {
    g_current_window = NULL;
  }

  free(window);
}

void Platform_SetWindowTitle(platform_window *window, const char *title) {
  if (window && window->xdg_toplevel) {
    xdg_toplevel_set_title(window->xdg_toplevel, title);
  }
}

void Platform_GetWindowSize(platform_window *window, i32 *width, i32 *height) {
  if (window) {
    if (width)
      *width = window->width;
    if (height)
      *height = window->height;
  }
}

b32 Platform_WindowShouldClose(platform_window *window) {
  return window ? window->should_close : true;
}

void *Platform_GetFramebuffer(platform_window *window) {
  if (!window)
    return NULL;
  /* Return back buffer (the one NOT currently displayed) */
  return window->shm_data[window->current_buffer];
}

void Platform_PresentFrame(platform_window *window) {
  if (!window || !window->surface)
    return;

  i32 buf_idx = window->current_buffer;
  if (!window->buffers[buf_idx])
    return;

  wl_surface_attach(window->surface, window->buffers[buf_idx], 0, 0);
  wl_surface_damage_buffer(window->surface, 0, 0, window->width,
                           window->height);
  wl_surface_commit(window->surface);

  /* Swap to other buffer for next frame */
  window->current_buffer = 1 - window->current_buffer;
}
