#ifdef _WIN32
/*
 * windows_platform.c - Windows platform initialization and cleanup
 *
 * C99, handmade hero style.
 */

#include "windows_internal.h"

windows_platform g_platform = {0};

/* ===== Cursor API ===== */

void Platform_SetCursor(cursor_type cursor) {
  HCURSOR hc = NULL;
  switch (cursor) {
  case WB_CURSOR_DEFAULT:
    hc = LoadCursor(NULL, IDC_ARROW);
    break;
  case WB_CURSOR_POINTER:
    hc = LoadCursor(NULL, IDC_HAND);
    break;
  case WB_CURSOR_TEXT:
    hc = LoadCursor(NULL, IDC_IBEAM);
    break;
  case WB_CURSOR_GRAB:
    hc = LoadCursor(NULL, IDC_HAND);
    break;
  case WB_CURSOR_GRABBING:
    hc = LoadCursor(NULL, IDC_SIZEALL);
    break;
  case WB_CURSOR_NO_DROP:
    hc = LoadCursor(NULL, IDC_NO);
    break;
  case WB_CURSOR_COPY:
    hc =
        LoadCursor(NULL, IDC_ARROW); /* IDC_COPY doesn't exist in system cursors
                                      */
    break;
  }
  if (hc) {
    SetCursor(hc);
  }
}

/* ===== Platform Init/Shutdown ===== */

b32 Platform_Init(void) {
  if (g_platform.initialized)
    return true;

  g_platform.instance = GetModuleHandle(NULL);
  if (!g_platform.instance) {
    return false;
  }

  HRESULT hr = OleInitialize(NULL);
  if (FAILED(hr)) {
    return false;
  }
  g_platform.ole_initialized = true;

  g_platform.initialized = true;
  return true;
}

void Platform_Shutdown(void) {
  if (!g_platform.initialized)
    return;

  if (g_platform.ole_initialized) {
    OleUninitialize();
  }

  memset(&g_platform, 0, sizeof(g_platform));
}
#endif /* _WIN32 */
