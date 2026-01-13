#ifdef _WIN32
/*
 * windows_time.c - Windows time and sleep functions
 *
 * Uses QueryPerformanceCounter for high-resolution timing.
 * C99, handmade hero style.
 */

#include "windows_internal.h"

static LARGE_INTEGER g_perf_frequency = {0};
static b32 g_frequency_initialized = false;

static void EnsureFrequencyInitialized(void) {
  if (!g_frequency_initialized) {
    QueryPerformanceFrequency(&g_perf_frequency);
    g_frequency_initialized = true;
  }
}

u64 Platform_GetTimeMs(void) {
  EnsureFrequencyInitialized();

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);

  return (u64)(counter.QuadPart * 1000 / g_perf_frequency.QuadPart);
}

void Platform_SleepMs(u32 ms) { Sleep(ms); }
#endif /* _WIN32 */
