/*
 * windows_fs_watcher.c - Windows File System Watcher Implementation
 *
 * Uses FindFirstChangeNotification for directory monitoring.
 * C99, handmade hero style.
 */

#include "../../core/fs_watcher.h"
#include "windows_internal.h"

b32 FSWatcher_Init(fs_watcher *watcher) {
  if (!watcher)
    return false;

  watcher->handle = INVALID_HANDLE_VALUE;
  watcher->path[0] = '\0';
  watcher->has_changes = false;

  return true;
}

void FSWatcher_Shutdown(fs_watcher *watcher) {
  if (!watcher)
    return;

  FSWatcher_StopWatching(watcher);
}

b32 FSWatcher_WatchDirectory(fs_watcher *watcher, const char *path) {
  if (!watcher || !path)
    return false;

  /* Stop watching previous directory if any */
  FSWatcher_StopWatching(watcher);

  /* Convert path to wide string */
  wchar_t wide_path[1024];
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, 1024) == 0) {
    return false;
  }

  /* Create change notification handle */
  watcher->handle = FindFirstChangeNotificationW(
      wide_path, FALSE, /* Don't watch subtree */
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
          FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);

  if (watcher->handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  /* Store path for reference */
  strncpy(watcher->path, path, sizeof(watcher->path) - 1);
  watcher->path[sizeof(watcher->path) - 1] = '\0';

  return true;
}

void FSWatcher_StopWatching(fs_watcher *watcher) {
  if (!watcher)
    return;

  if (watcher->handle != INVALID_HANDLE_VALUE) {
    FindCloseChangeNotification(watcher->handle);
    watcher->handle = INVALID_HANDLE_VALUE;
  }

  watcher->path[0] = '\0';
  watcher->has_changes = false;
}

b32 FSWatcher_Poll(fs_watcher *watcher) {
  if (!watcher || watcher->handle == INVALID_HANDLE_VALUE)
    return false;

  /* Wait with 0 timeout for non-blocking check */
  DWORD result = WaitForSingleObject(watcher->handle, 0);

  if (result == WAIT_OBJECT_0) {
    /* Change detected - reset the notification */
    FindNextChangeNotification(watcher->handle);
    return true;
  }

  return false;
}
