/*
 * windows_window.c - Windows window management with software rendering
 *
 * Creates a Win32 window with a DIB section for software rendering.
 * C99, handmade hero style.
 */

#include "windows_internal.h"

static const wchar_t *WINDOW_CLASS_NAME = L"WorkbenchWindowClass";
static b32 g_class_registered = false;

/* Forward declaration of window procedure */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* ===== Internal Helpers ===== */

static b32 RegisterWindowClass(void) {
  if (g_class_registered)
    return true;

  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = g_platform.instance;
  wc.hIcon = LoadIcon(g_platform.instance, MAKEINTRESOURCE(1));
  wc.hIconSm = LoadIcon(g_platform.instance, MAKEINTRESOURCE(1));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = WINDOW_CLASS_NAME;

  if (!RegisterClassExW(&wc))
    return false;

  g_class_registered = true;
  return true;
}

b32 CreateFramebuffer(platform_window *window) {
  /* Create DIB section for software rendering */
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = window->width;
  bmi.bmiHeader.biHeight = -window->height; /* Top-down DIB */
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  window->mem_dc = CreateCompatibleDC(window->hdc);
  if (!window->mem_dc)
    return false;

  window->bitmap = CreateDIBSection(window->mem_dc, &bmi, DIB_RGB_COLORS,
                                    &window->framebuffer, NULL, 0);
  if (!window->bitmap) {
    DeleteDC(window->mem_dc);
    window->mem_dc = NULL;
    return false;
  }

  window->old_bitmap = SelectObject(window->mem_dc, window->bitmap);

  /* Clear to dark background */
  u32 *pixels = (u32 *)window->framebuffer;
  for (i32 i = 0; i < window->width * window->height; i++) {
    pixels[i] = 0xFF1E1E2E; /* Dark theme background (BGRA format) */
  }

  return true;
}

void DestroyFramebuffer(platform_window *window) {
  if (window->old_bitmap) {
    SelectObject(window->mem_dc, window->old_bitmap);
    window->old_bitmap = NULL;
  }
  if (window->bitmap) {
    DeleteObject(window->bitmap);
    window->bitmap = NULL;
  }
  if (window->mem_dc) {
    DeleteDC(window->mem_dc);
    window->mem_dc = NULL;
  }
  window->framebuffer = NULL;
}

/* ===== Window API ===== */

platform_window *Platform_CreateWindow(window_config *config) {
  if (!g_platform.initialized) {
    if (!Platform_Init())
      return NULL;
  }

  if (!RegisterWindowClass())
    return NULL;

  platform_window *window = calloc(1, sizeof(platform_window));
  if (!window)
    return NULL;

  window->width = config->width;
  window->height = config->height;
  window->resizable = config->resizable;

  /* Calculate window size including non-client area */
  DWORD style = WS_OVERLAPPEDWINDOW;
  if (!config->resizable) {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }

  RECT rect = {0, 0, config->width, config->height};
  AdjustWindowRect(&rect, style, FALSE);

  i32 window_width = rect.right - rect.left;
  i32 window_height = rect.bottom - rect.top;

  /* Convert title to wide string */
  wchar_t wide_title[256];
  MultiByteToWideChar(CP_UTF8, 0, config->title, -1, wide_title, 256);

  window->hwnd = CreateWindowExW(0,                   /* Extended style */
                                 WINDOW_CLASS_NAME,   /* Class name */
                                 wide_title,          /* Window title */
                                 style,               /* Style */
                                 CW_USEDEFAULT,       /* X position */
                                 CW_USEDEFAULT,       /* Y position */
                                 window_width,        /* Width */
                                 window_height,       /* Height */
                                 NULL,                /* Parent */
                                 NULL,                /* Menu */
                                 g_platform.instance, /* Instance */
                                 window               /* User data */
  );

  if (!window->hwnd) {
    free(window);
    return NULL;
  }

  /* Store window pointer in GWLP_USERDATA for WndProc access */
  SetWindowLongPtrW(window->hwnd, GWLP_USERDATA, (LONG_PTR)window);

  window->hdc = GetDC(window->hwnd);
  if (!window->hdc) {
    DestroyWindow(window->hwnd);
    free(window);
    return NULL;
  }

  if (!CreateFramebuffer(window)) {
    ReleaseDC(window->hwnd, window->hdc);
    DestroyWindow(window->hwnd);
    free(window);
    return NULL;
  }

  ShowWindow(window->hwnd, config->maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
  UpdateWindow(window->hwnd);

  return window;
}

void Platform_DestroyWindow(platform_window *window) {
  if (!window)
    return;

  DestroyFramebuffer(window);

  if (window->hdc) {
    ReleaseDC(window->hwnd, window->hdc);
  }

  if (window->hwnd) {
    DestroyWindow(window->hwnd);
  }

  free(window);
}

void Platform_SetWindowTitle(platform_window *window, const char *title) {
  if (!window || !window->hwnd)
    return;
  wchar_t wide_title[256];
  MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, 256);
  SetWindowTextW(window->hwnd, wide_title);
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
  return window->framebuffer;
}

void Platform_PresentFrame(platform_window *window) {
  if (!window || !window->hwnd || !window->mem_dc)
    return;

  /* Blit framebuffer to window */
  BitBlt(window->hdc, 0, 0, window->width, window->height, window->mem_dc, 0, 0,
         SRCCOPY);
}
