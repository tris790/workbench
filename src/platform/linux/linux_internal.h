/*
 * linux_internal.h - Internal shared state for Linux platform
 */

#ifndef LINUX_INTERNAL_H
#define LINUX_INTERNAL_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700 // For strdup

#include "../platform.h"
#include "../protocols/xdg-decoration-client-protocol.h"
#include "../protocols/xdg-shell-client-protocol.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

/* ===== File Clipboard Support ===== */

#define CLIPBOARD_MAX_FILES 64

typedef struct {
  char paths[CLIPBOARD_MAX_FILES][FS_MAX_PATH];
  i32 count;
  b32 is_cut; /* true = move on paste, false = copy on paste */
} clipboard_files;

/* ===== Platform State ===== */

typedef struct {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_wm_base *xdg_wm_base;
  struct zxdg_decoration_manager_v1 *decoration_manager;
  struct wl_seat *seat;
  struct wl_keyboard *keyboard;
  struct wl_pointer *pointer;
  struct wl_data_device_manager *data_device_manager;
  struct wl_data_device *data_device;
  struct wl_data_source *clipboard_source;
  char *clipboard_content;
  struct wl_data_offer *selection_offer;
  u32 last_serial;
  b32 initialized;
  
  /* File clipboard state */
  clipboard_files file_clipboard;
} linux_platform;

extern linux_platform g_platform;

/* ===== Window Structure ===== */

struct platform_window {
  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct zxdg_toplevel_decoration_v1 *decoration;

  /* Double-buffered shared memory for rendering */
  struct wl_buffer *buffers[2];
  void *shm_data[2];
  usize shm_size;
  i32 current_buffer; /* Index of buffer being drawn to */

  /* Window state */
  i32 width;
  i32 height;
  b32 should_close;
  b32 configured;
  b32 resizable;
  b32 fullscreen;

  /* Event queue */
  platform_event events[64];
  usize event_read;
  usize event_write;

  /* Input state */
  i32 mouse_x, mouse_y;
  u32 modifiers;
};

/* ===== Internal Helpers ===== */

void PushEvent(platform_window *window, platform_event *event);
i32 CreateShmBuffer(platform_window *window, i32 index);
void DestroyShmBuffers(platform_window *window);

#endif /* LINUX_INTERNAL_H */
