/*
 * fs_watcher.h - File System Watcher for Workbench
 *
 * Platform-agnostic API for monitoring directory changes.
 * Used to detect external file operations (terminal, other apps).
 * C99, handmade hero style.
 */

#ifndef FS_WATCHER_H
#define FS_WATCHER_H

#include "types.h"

/* ===== File Watcher Types ===== */

/* Opaque watcher handle - one per explorer panel */
typedef struct fs_watcher {
  i32 fd;          /* inotify file descriptor (Linux) */
  i32 wd;          /* Current watch descriptor */
  char path[512];  /* Currently watched path */
  b32 has_changes; /* Flag set when changes detected */
} fs_watcher;

/* ===== File Watcher API ===== */

/* Initialize a file watcher instance */
b32 FSWatcher_Init(fs_watcher *watcher);

/* Shutdown and cleanup watcher resources */
void FSWatcher_Shutdown(fs_watcher *watcher);

/* Start watching a directory (stops watching previous if any) */
b32 FSWatcher_WatchDirectory(fs_watcher *watcher, const char *path);

/* Stop watching current directory */
void FSWatcher_StopWatching(fs_watcher *watcher);

/* Poll for changes (non-blocking, call each frame)
 * Returns true if any changes detected since last poll */
b32 FSWatcher_Poll(fs_watcher *watcher);

#endif /* FS_WATCHER_H */
