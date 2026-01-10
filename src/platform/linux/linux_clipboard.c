/*
 * linux_clipboard.c - Linux/Wayland clipboard implementation
 */

#include "linux_internal.h"

/* ===== Clipboard Callbacks ===== */

static void DataSourceTarget(void *data, struct wl_data_source *source,
                             const char *mime_type) {
  (void)data; (void)source; (void)mime_type;
}

static void DataSourceSend(void *data, struct wl_data_source *source,
                           const char *mime_type, i32 fd) {
  (void)data; (void)source;
  if (strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
      strcmp(mime_type, "text/plain") == 0) {
    if (g_platform.clipboard_content) {
      write(fd, g_platform.clipboard_content,
            strlen(g_platform.clipboard_content));
    }
  }
  close(fd);
}

static void DataSourceCancelled(void *data, struct wl_data_source *source) {
  (void)data;
  if (g_platform.clipboard_source == source) {
    g_platform.clipboard_source = NULL;
  }
  wl_data_source_destroy(source);
}

static void DataSourceDndDropPerformed(void *data, struct wl_data_source *source) {
  (void)data; (void)source;
}

static void DataSourceDndFinished(void *data, struct wl_data_source *source) {
  (void)data; (void)source;
}

static void DataSourceAction(void *data, struct wl_data_source *source, u32 dnd_action) {
  (void)data; (void)source; (void)dnd_action;
}

static const struct wl_data_source_listener data_source_listener = {
    .target = DataSourceTarget,
    .send = DataSourceSend,
    .cancelled = DataSourceCancelled,
    .dnd_drop_performed = DataSourceDndDropPerformed,
    .dnd_finished = DataSourceDndFinished,
    .action = DataSourceAction,
};

static void DataOfferOffer(void *data, struct wl_data_offer *offer,
                           const char *mime_type) {
  (void)data;
  if (strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
      strcmp(mime_type, "text/plain") == 0) {
    /* Mark offer as containing text */
    wl_data_offer_set_user_data(offer, (void *)(intptr_t)1);
  }
}

static void DataOfferSourceActions(void *data, struct wl_data_offer *offer, u32 source_actions) {
  (void)data; (void)offer; (void)source_actions;
}

static void DataOfferAction(void *data, struct wl_data_offer *offer, u32 dnd_action) {
  (void)data; (void)offer; (void)dnd_action;
}

const struct wl_data_offer_listener data_offer_listener = {
    .offer = DataOfferOffer,
    .source_actions = DataOfferSourceActions,
    .action = DataOfferAction,
};

/* ===== Data Device Callbacks ===== */
/* These were formerly locally static in platform_linux.c but needed here and linked */

static void DataDeviceDataOffer(void *data, struct wl_data_device *device,
                                struct wl_data_offer *offer) {
  (void)data; (void)device;
  wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
}

static void DataDeviceEnter(void *data, struct wl_data_device *device,
                            u32 serial, struct wl_surface *surface,
                            wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id) {
  (void)data; (void)device; (void)serial; (void)surface; (void)x; (void)y; (void)id;
}

static void DataDeviceLeave(void *data, struct wl_data_device *device) {
  (void)data; (void)device;
}

static void DataDeviceMotion(void *data, struct wl_data_device *device,
                             u32 time, wl_fixed_t x, wl_fixed_t y) {
  (void)data; (void)device; (void)time; (void)x; (void)y;
}

static void DataDeviceDrop(void *data, struct wl_data_device *device) {
  (void)data; (void)device;
}

static void DataDeviceSelection(void *data, struct wl_data_device *device,
                                struct wl_data_offer *offer) {
  (void)data; (void)device;
  if (g_platform.selection_offer) {
    wl_data_offer_destroy(g_platform.selection_offer);
  }
  g_platform.selection_offer = offer;
}

const struct wl_data_device_listener data_device_listener = {
    .data_offer = DataDeviceDataOffer,
    .enter = DataDeviceEnter,
    .leave = DataDeviceLeave,
    .motion = DataDeviceMotion,
    .drop = DataDeviceDrop,
    .selection = DataDeviceSelection,
};

/* ===== Clipboard API ===== */

char *Platform_GetClipboard(memory_arena *arena) {
  if (!g_platform.selection_offer) {
    return NULL;
  }

  /* Check if text is available */
  if ((intptr_t)wl_data_offer_get_user_data(g_platform.selection_offer) != 1) {
    return NULL;
  }

  int fds[2];
  if (pipe(fds) != 0) {
    return NULL;
  }

  wl_data_offer_receive(g_platform.selection_offer, "text/plain", fds[1]);
  close(fds[1]);

  /* Flush request to server */
  wl_display_roundtrip(g_platform.display);

  /* Read from pipe */
  usize capacity = 1024;
  char *buffer = malloc(capacity);
  usize len = 0;

  while (true) {
    ssize_t r = read(fds[0], buffer + len, capacity - len - 1);
    if (r < 0) {
        break;
    }
    if (r == 0) {
        break; /* EOF */
    }
    len += r;
    if (len >= capacity - 1) {
      capacity *= 2;
      buffer = realloc(buffer, capacity);
    }
  }
  buffer[len] = '\0';
  close(fds[0]);

  /* Copy to arena */
  char *result = ArenaPushArray(arena, char, len + 1);
  memcpy(result, buffer, len + 1);
  free(buffer);

  return result;
}

b32 Platform_SetClipboard(const char *text) {
  if (!g_platform.data_device_manager || !g_platform.seat) {
    return false;
  }

  /* Update stored content */
  if (g_platform.clipboard_content) {
    free(g_platform.clipboard_content);
  }
  g_platform.clipboard_content = strdup(text);

  /* Create new source */
  if (g_platform.clipboard_source) {
      wl_data_source_destroy(g_platform.clipboard_source);
  }

  g_platform.clipboard_source = wl_data_device_manager_create_data_source(g_platform.data_device_manager);
  wl_data_source_add_listener(g_platform.clipboard_source, &data_source_listener, NULL);
  
  wl_data_source_offer(g_platform.clipboard_source, "text/plain;charset=utf-8");
  wl_data_source_offer(g_platform.clipboard_source, "text/plain");

  wl_data_device_set_selection(g_platform.data_device, g_platform.clipboard_source, g_platform.last_serial); 
  
  return true;
}
