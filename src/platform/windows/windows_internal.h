/*
 * windows_internal.h - Internal shared state for Windows platform
 *
 * Mirrors linux_internal.h structure for consistency.
 * C99, handmade hero style.
 */

#ifndef WINDOWS_INTERNAL_H
#define WINDOWS_INTERNAL_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../platform.h"
#include <windows.h>
#ifndef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT __declspec(dllimport)
#endif
#include <shellapi.h>

/* ===== Platform State ===== */

typedef struct {
  HINSTANCE instance;
  b32 initialized;
} windows_platform;

extern windows_platform g_platform;

/* ===== Window Structure ===== */

struct platform_window {
  HWND hwnd;
  HDC hdc;
  HDC mem_dc;
  HBITMAP bitmap;
  HBITMAP old_bitmap;
  void *framebuffer;

  /* Window state */
  i32 width;
  i32 height;
  b32 should_close;
  b32 resizable;
  b32 is_fullscreen;
  WINDOWPLACEMENT prev_placement;

  /* Event queue */
  /* Event queue */
#define EVENT_QUEUE_SIZE 1024
  platform_event events[EVENT_QUEUE_SIZE];
  usize event_read;
  usize event_write;

  /* Input state */
  i32 mouse_x, mouse_y;
  u32 modifiers;
  WPARAM last_vk;
};

/* ===== Internal Helpers ===== */

void PushEvent(platform_window *window, platform_event *event);
key_code VirtualKeyToKeyCode(WPARAM vk);
u32 GetModifierState(void);

/* Framebuffer management */
b32 CreateFramebuffer(platform_window *window);
void DestroyFramebuffer(platform_window *window);

#endif /* WINDOWS_INTERNAL_H */
