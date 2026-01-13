#ifdef _WIN32
/*
 * windows_platform.c - Windows platform initialization and cleanup
 *
 * C99, handmade hero style.
 */

#include "windows_internal.h"

windows_platform g_platform = {0};

/* ===== Platform Init/Shutdown ===== */

b32 Platform_Init(void) {
  if (g_platform.initialized)
    return true;

  g_platform.instance = GetModuleHandle(NULL);
  if (!g_platform.instance) {
    return false;
  }

  g_platform.initialized = true;
  return true;
}

void Platform_Shutdown(void) {
  if (!g_platform.initialized)
    return;

  memset(&g_platform, 0, sizeof(g_platform));
}
#endif /* _WIN32 */
