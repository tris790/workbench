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
  } else if (strcmp(mime_type, "text/uri-list") == 0) {
    /* Send file URIs */
    clipboard_files *files = &g_platform.file_clipboard;
    if (files->count > 0) {
      char uri[FS_MAX_PATH + 16];
      for (i32 i = 0; i < files->count; i++) {
        /* Convert path to file:// URI */
        /* Handle paths that may already have file:// prefix */
        const char *path = files->paths[i];
        if (strncmp(path, "file://", 7) == 0) {
          snprintf(uri, sizeof(uri), "%s\r\n", path);
        } else {
          snprintf(uri, sizeof(uri), "file://%s\r\n", path);
        }
        write(fd, uri, strlen(uri));
      }
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
    /* Mark offer as containing text (1 = text, 2 = files, 3 = both) */
    intptr_t existing = (intptr_t)wl_data_offer_get_user_data(offer);
    wl_data_offer_set_user_data(offer, (void *)(existing | 1));
  } else if (strcmp(mime_type, "text/uri-list") == 0) {
    /* Mark offer as containing files */
    intptr_t existing = (intptr_t)wl_data_offer_get_user_data(offer);
    wl_data_offer_set_user_data(offer, (void *)(existing | 2));
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

char *Platform_GetClipboard(char *buffer, usize buffer_size) {
  if (!g_platform.selection_offer) {
    return NULL;
  }

  /* Check if text is available (bit 0 set = text available) */
  intptr_t user_data = (intptr_t)wl_data_offer_get_user_data(g_platform.selection_offer);
  if ((user_data & 1) == 0) {
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
  usize len = 0;
  
  while (len < buffer_size - 1) {
    ssize_t r = read(fds[0], buffer + len, buffer_size - len - 1);
    if (r < 0) {
        break;
    }
    if (r == 0) {
        break; /* EOF */
    }
    len += r;
  }
  buffer[len] = '\0';
  close(fds[0]);

  return buffer;
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

/* ===== File Clipboard API ===== */

b32 Platform_ClipboardSetFiles(const char **paths, i32 count, b32 is_cut) {
  (void)is_cut; /* Cut vs copy is handled by the app on paste, not by Wayland protocol */
  
  if (!g_platform.data_device_manager || !g_platform.seat) {
    return false;
  }
  
  if (count <= 0 || count > CLIPBOARD_MAX_FILES) {
    return false;
  }
  
  /* Store files in our clipboard state */
  clipboard_files *files = &g_platform.file_clipboard;
  files->count = count;
  files->is_cut = is_cut;
  
  for (i32 i = 0; i < count; i++) {
    strncpy(files->paths[i], paths[i], FS_MAX_PATH - 1);
    files->paths[i][FS_MAX_PATH - 1] = '\0';
  }
  
  /* Clear text content when setting files */
  if (g_platform.clipboard_content) {
    free(g_platform.clipboard_content);
    g_platform.clipboard_content = NULL;
  }
  
  /* Create new source for file data */
  if (g_platform.clipboard_source) {
    wl_data_source_destroy(g_platform.clipboard_source);
  }
  
  g_platform.clipboard_source = wl_data_device_manager_create_data_source(g_platform.data_device_manager);
  wl_data_source_add_listener(g_platform.clipboard_source, &data_source_listener, NULL);
  
  /* Offer text/uri-list MIME type */
  wl_data_source_offer(g_platform.clipboard_source, "text/uri-list");
  
  wl_data_device_set_selection(g_platform.data_device, g_platform.clipboard_source, g_platform.last_serial);
  
  return true;
}

i32 Platform_ClipboardGetFiles(char **paths_out, i32 max_paths, b32 *is_cut_out) {
  *is_cut_out = false; /* Wayland doesn't distinguish cut vs copy in protocol */
  
  if (!g_platform.selection_offer) {
    return 0;
  }
  
  /* Check if files are available (bit 1 set = files available) */
  intptr_t user_data = (intptr_t)wl_data_offer_get_user_data(g_platform.selection_offer);
  if ((user_data & 2) == 0) {
    return 0;
  }
  
  int fds[2];
  if (pipe(fds) != 0) {
    return 0;
  }
  
  wl_data_offer_receive(g_platform.selection_offer, "text/uri-list", fds[1]);
  close(fds[1]);
  
  /* Flush request to server */
  wl_display_roundtrip(g_platform.display);
  
  /* Read uri-list from pipe */
  char buffer[65536];
  usize total_len = 0;
  
  while (total_len < sizeof(buffer) - 1) {
    ssize_t r = read(fds[0], buffer + total_len, sizeof(buffer) - total_len - 1);
    if (r <= 0) break;
    total_len += r;
  }
  buffer[total_len] = '\0';
  close(fds[0]);
  
  /* Parse uri-list (format: file:///path/to/file\r\n or #comments) */
  i32 count = 0;
  char *line = buffer;
  
  while (count < max_paths && *line) {
    /* Find end of line */
    char *end = strstr(line, "\r\n");
    if (!end) end = line + strlen(line);
    
    /* Skip empty lines and comments */
    if (line[0] != '\0' && line[0] != '#' && line[0] != '\r' && line[0] != '\n') {
      /* Temporarily null terminate */
      char saved = *end;
      *end = '\0';
      
      /* Extract path from file:// URI */
      const char *path = line;
      if (strncmp(line, "file://", 7) == 0) {
        path = line + 7;
      }
      
      /* Copy path, ensuring null termination without truncation warnings */
      usize path_len = strlen(path);
      if (path_len >= FS_MAX_PATH) {
        path_len = FS_MAX_PATH - 1;
      }
      memcpy(paths_out[count], path, path_len);
      paths_out[count][path_len] = '\0';
      count++;
      
      *end = saved;
    }
    
    /* Move to next line */
    if (end[0] == '\r' && end[1] == '\n') {
      line = end + 2;
    } else {
      break;
    }
  }
  
  return count;
}
