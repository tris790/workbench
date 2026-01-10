/*
 * linux_time.c - Linux time and sleep functions
 */

#include "linux_internal.h"
#include <time.h>

u64 Platform_GetTimeMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000 + (u64)ts.tv_nsec / 1000000;
}

void Platform_SleepMs(u32 ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, NULL);
}
