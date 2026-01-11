/*
 * linux_fs_watcher.c - Linux File System Watcher Implementation
 *
 * Uses inotify for efficient, non-blocking file system monitoring.
 * C99, handmade hero style.
 */

#include "fs_watcher.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

/* Events we care about for file explorer refresh */
#define WATCH_EVENTS                                                           \
  (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY |           \
   IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF)

/* Buffer size for reading inotify events */
#define EVENT_BUF_SIZE (1024 * (sizeof(struct inotify_event) + 256))

b32 FSWatcher_Init(fs_watcher *watcher) {
  if (!watcher)
    return false;

  /* Initialize with IN_NONBLOCK for non-blocking reads */
  watcher->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (watcher->fd < 0) {
    fprintf(stderr, "FSWatcher: inotify_init1 failed: %s\n", strerror(errno));
    return false;
  }

  watcher->wd = -1;
  watcher->path[0] = '\0';
  watcher->has_changes = false;

  return true;
}

void FSWatcher_Shutdown(fs_watcher *watcher) {
  if (!watcher)
    return;

  FSWatcher_StopWatching(watcher);

  if (watcher->fd >= 0) {
    close(watcher->fd);
    watcher->fd = -1;
  }
}

b32 FSWatcher_WatchDirectory(fs_watcher *watcher, const char *path) {
  if (!watcher || !path || watcher->fd < 0)
    return false;

  /* Stop watching previous directory if any */
  FSWatcher_StopWatching(watcher);

  /* Add watch for new directory */
  watcher->wd = inotify_add_watch(watcher->fd, path, WATCH_EVENTS);
  if (watcher->wd < 0) {
    fprintf(stderr, "FSWatcher: inotify_add_watch failed for '%s': %s\n", path,
            strerror(errno));
    return false;
  }

  /* Store path for reference */
  strncpy(watcher->path, path, sizeof(watcher->path) - 1);
  watcher->path[sizeof(watcher->path) - 1] = '\0';

  return true;
}

void FSWatcher_StopWatching(fs_watcher *watcher) {
  if (!watcher || watcher->fd < 0)
    return;

  if (watcher->wd >= 0) {
    inotify_rm_watch(watcher->fd, watcher->wd);
    watcher->wd = -1;
  }

  watcher->path[0] = '\0';
  watcher->has_changes = false;
}

b32 FSWatcher_Poll(fs_watcher *watcher) {
  if (!watcher || watcher->fd < 0 || watcher->wd < 0)
    return false;

  /* Use poll with 0 timeout for non-blocking check */
  struct pollfd pfd = {
      .fd = watcher->fd,
      .events = POLLIN,
      .revents = 0,
  };

  int ret = poll(&pfd, 1, 0); /* 0ms timeout = non-blocking */

  if (ret < 0) {
    if (errno != EINTR) {
      fprintf(stderr, "FSWatcher: poll failed: %s\n", strerror(errno));
    }
    return false;
  }

  if (ret == 0 || !(pfd.revents & POLLIN)) {
    /* No events available */
    return false;
  }

  /* Read and consume all available events */
  char buf[EVENT_BUF_SIZE];
  b32 changes_detected = false;

  while (1) {
    ssize_t len = read(watcher->fd, buf, sizeof(buf));

    if (len < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* No more events available */
        break;
      }
      fprintf(stderr, "FSWatcher: read failed: %s\n", strerror(errno));
      break;
    }

    if (len == 0) {
      break;
    }

    /* Process events - we don't need to parse them individually,
     * any event means we should refresh */
    changes_detected = true;

    /* Parse to check for watch removal (directory deleted/moved) */
    ssize_t i = 0;
    while (i < len) {
      struct inotify_event *event = (struct inotify_event *)&buf[i];

      /* Only invalidate if event is for our CURRENT watch descriptor.
       * IN_IGNORED events from old watches (after navigation) should be
       * ignored. */
      if ((event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED)) &&
          event->wd == watcher->wd) {
        /* Watched directory was deleted or moved - watch is invalid */
        watcher->wd = -1;
      }

      i += sizeof(struct inotify_event) + event->len;
    }
  }

  return changes_detected;
}
