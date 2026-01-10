/*
 * platform_linux.c - Linux/Wayland platform implementation for Workbench
 *
 * Native Wayland using xdg-shell protocol for window management.
 * C99, handmade hero style.
 */

#define _POSIX_C_SOURCE 200809L

#include "platform.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "xdg-decoration-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <wayland-client.h>

/* ===== Platform State ===== */

typedef struct {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_wm_base *xdg_wm_base;
  struct zxdg_decoration_manager_v1 *decoration_manager;
  struct wl_seat *seat;
  struct wl_keyboard *keyboard;
  struct wl_pointer *pointer;
  struct wl_data_device_manager *data_device_manager;
  struct wl_data_device *data_device;
  struct wl_data_source *clipboard_source;
  char *clipboard_content;
  struct wl_data_offer *selection_offer;
  u32 last_serial;
  b32 initialized;
} linux_platform;

static linux_platform g_platform = {0};

/* ===== Window Structure ===== */

struct platform_window {
  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct zxdg_toplevel_decoration_v1 *decoration;

  /* Double-buffered shared memory for rendering */
  struct wl_buffer *buffers[2];
  void *shm_data[2];
  usize shm_size;
  i32 current_buffer; /* Index of buffer being drawn to */

  /* Window state */
  i32 width;
  i32 height;
  b32 should_close;
  b32 configured;
  b32 resizable;

  /* Event queue */
  platform_event events[64];
  usize event_read;
  usize event_write;

  /* Input state */
  i32 mouse_x, mouse_y;
  u32 modifiers;
};

/* ===== Event Queue Helpers ===== */

static void PushEvent(platform_window *window, platform_event *event) {
  usize next = (window->event_write + 1) % ArrayCount(window->events);
  if (next != window->event_read) {
    window->events[window->event_write] = *event;
    window->event_write = next;
  }
}

/* ===== XDG Shell Callbacks ===== */

static void XdgWmBasePing(void *data, struct xdg_wm_base *xdg_wm_base,
                          u32 serial) {
  (void)data;
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = XdgWmBasePing,
};

static void XdgSurfaceConfigure(void *data, struct xdg_surface *xdg_surface,
                                u32 serial) {
  platform_window *window = (platform_window *)data;
  xdg_surface_ack_configure(xdg_surface, serial);
  window->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = XdgSurfaceConfigure,
};

static void DestroyShmBuffers(platform_window *window) {
  for (i32 i = 0; i < 2; i++) {
    if (window->shm_data[i] && window->shm_data[i] != MAP_FAILED) {
      munmap(window->shm_data[i], window->shm_size);
      window->shm_data[i] = NULL;
    }
    if (window->buffers[i]) {
      wl_buffer_destroy(window->buffers[i]);
      window->buffers[i] = NULL;
    }
  }
}

static i32 CreateShmBuffer(platform_window *window, i32 index);

static void XdgToplevelConfigure(void *data, struct xdg_toplevel *toplevel,
                                 i32 width, i32 height,
                                 struct wl_array *states) {
  (void)toplevel;
  (void)states;
  platform_window *window = (platform_window *)data;

  if (width > 0 && height > 0) {
    if (window->width != width || window->height != height) {
      
      /* Resize buffers */
      DestroyShmBuffers(window);
      
      window->width = width;
      window->height = height;
      
      CreateShmBuffer(window, 0);
      CreateShmBuffer(window, 1);

      platform_event event = {0};
      event.type = EVENT_WINDOW_RESIZE;
      event.data.resize.width = width;
      event.data.resize.height = height;
      PushEvent(window, &event);
    }
  }
}

static void XdgToplevelClose(void *data, struct xdg_toplevel *toplevel) {
  (void)toplevel;
  platform_window *window = (platform_window *)data;
  window->should_close = true;

  platform_event event = {0};
  event.type = EVENT_QUIT;
  PushEvent(window, &event);
}

static void XdgToplevelConfigureBounds(void *data,
                                       struct xdg_toplevel *toplevel, i32 width,
                                       i32 height) {
  (void)data;
  (void)toplevel;
  (void)width;
  (void)height;
}

static void XdgToplevelWmCapabilities(void *data, struct xdg_toplevel *toplevel,
                                      struct wl_array *capabilities) {
  (void)data;
  (void)toplevel;
  (void)capabilities;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = XdgToplevelConfigure,
    .close = XdgToplevelClose,
    .configure_bounds = XdgToplevelConfigureBounds,
    .wm_capabilities = XdgToplevelWmCapabilities,
};

/* ===== Keyboard Callbacks ===== */

static key_code LinuxKeyToKeyCode(u32 key);

static u32 KeyCodeToChar(key_code key, u32 modifiers) {
  if (modifiers & MOD_CTRL) {
    return 0;
  }
  b32 shift = (modifiers & MOD_SHIFT) != 0;

  if (key >= KEY_A && key <= KEY_Z) {
    if (shift)
      return 'A' + (key - KEY_A);
    return 'a' + (key - KEY_A);
  }

  if (key >= KEY_0 && key <= KEY_9) {
    if (shift) {
      const char *syms = ")!@#$%^&*(";
      return syms[key - KEY_0];
    }
    return '0' + (key - KEY_0);
  }

  switch (key) {
  case KEY_SPACE:
    return ' ';
  case KEY_GRAVE:
    return shift ? '~' : '`';
  case KEY_MINUS:
    return shift ? '_' : '-';
  case KEY_EQUALS:
    return shift ? '+' : '=';
  case KEY_LEFTBRACKET:
    return shift ? '{' : '[';
  case KEY_RIGHTBRACKET:
    return shift ? '}' : ']';
  case KEY_BACKSLASH:
    return shift ? '|' : '\\';
  case KEY_SEMICOLON:
    return shift ? ':' : ';';
  case KEY_APOSTROPHE:
    return shift ? '"' : '\'';
  case KEY_COMMA:
    return shift ? '<' : ',';
  case KEY_PERIOD:
    return shift ? '>' : '.';
  case KEY_SLASH:
    return shift ? '?' : '/';
  default:
    return 0;
  }
}

static void KeyboardKeymap(void *data, struct wl_keyboard *keyboard, u32 format,
                           i32 fd, u32 size) {
  (void)data;
  (void)keyboard;
  (void)format;
  (void)size;
  close(fd);
}

static void KeyboardEnter(void *data, struct wl_keyboard *keyboard, u32 serial,
                          struct wl_surface *surface, struct wl_array *keys) {
  (void)data;
  (void)keyboard;
  (void)surface;
  (void)keys;
  g_platform.last_serial = serial;
}

static void KeyboardLeave(void *data, struct wl_keyboard *keyboard, u32 serial,
                          struct wl_surface *surface) {
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)surface;
}

static void KeyboardKey(void *data, struct wl_keyboard *keyboard, u32 serial,
                        u32 time, u32 key, u32 state) {
  (void)keyboard;
  (void)time;
  g_platform.last_serial = serial;
  platform_window *window = (platform_window *)data;

  platform_event event = {0};
  event.type =
      (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? EVENT_KEY_DOWN : EVENT_KEY_UP;
  event.data.keyboard.key = LinuxKeyToKeyCode(key);
  event.data.keyboard.modifiers = window->modifiers;
  if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    event.data.keyboard.character =
        KeyCodeToChar(event.data.keyboard.key, window->modifiers);
  }
  PushEvent(window, &event);
}

static void KeyboardModifiers(void *data, struct wl_keyboard *keyboard,
                              u32 serial, u32 mods_depressed, u32 mods_latched,
                              u32 mods_locked, u32 group) {
  (void)keyboard;
  (void)serial;
  (void)mods_latched;
  (void)mods_locked;
  (void)group;
  platform_window *window = (platform_window *)data;

  window->modifiers = 0;
  if (mods_depressed & 1)
    window->modifiers |= MOD_SHIFT;
  if (mods_depressed & 4)
    window->modifiers |= MOD_CTRL;
  if (mods_depressed & 8)
    window->modifiers |= MOD_ALT;
  if (mods_depressed & 64)
    window->modifiers |= MOD_SUPER;
}

static void KeyboardRepeatInfo(void *data, struct wl_keyboard *keyboard,
                               i32 rate, i32 delay) {
  (void)data;
  (void)keyboard;
  (void)rate;
  (void)delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = KeyboardKeymap,
    .enter = KeyboardEnter,
    .leave = KeyboardLeave,
    .key = KeyboardKey,
    .modifiers = KeyboardModifiers,
    .repeat_info = KeyboardRepeatInfo,
};

/* ===== Pointer Callbacks ===== */

static void PointerEnter(void *data, struct wl_pointer *pointer, u32 serial,
                         struct wl_surface *surface, wl_fixed_t sx,
                         wl_fixed_t sy) {
  (void)pointer;
  (void)surface;
  g_platform.last_serial = serial;
  platform_window *window = (platform_window *)data;
  window->mouse_x = wl_fixed_to_int(sx);
  window->mouse_y = wl_fixed_to_int(sy);
}

static void PointerLeave(void *data, struct wl_pointer *pointer, u32 serial,
                         struct wl_surface *surface) {
  (void)data;
  (void)pointer;
  (void)serial;
  (void)surface;
}

static void PointerMotion(void *data, struct wl_pointer *pointer, u32 time,
                          wl_fixed_t sx, wl_fixed_t sy) {
  (void)pointer;
  (void)time;
  platform_window *window = (platform_window *)data;
  window->mouse_x = wl_fixed_to_int(sx);
  window->mouse_y = wl_fixed_to_int(sy);

  platform_event event = {0};
  event.type = EVENT_MOUSE_MOVE;
  event.data.mouse.x = window->mouse_x;
  event.data.mouse.y = window->mouse_y;
  event.data.mouse.modifiers = window->modifiers;
  PushEvent(window, &event);
}

static void PointerButton(void *data, struct wl_pointer *pointer, u32 serial,
                          u32 time, u32 button, u32 state) {
  (void)pointer;
  (void)time;
  g_platform.last_serial = serial;
  platform_window *window = (platform_window *)data;

  platform_event event = {0};
  event.type = (state == WL_POINTER_BUTTON_STATE_PRESSED)
                   ? EVENT_MOUSE_BUTTON_DOWN
                   : EVENT_MOUSE_BUTTON_UP;

  /* Linux button codes: 272=left, 273=right, 274=middle */
  if (button == 272)
    event.data.mouse.button = MOUSE_LEFT;
  else if (button == 273)
    event.data.mouse.button = MOUSE_RIGHT;
  else if (button == 274)
    event.data.mouse.button = MOUSE_MIDDLE;

  event.data.mouse.x = window->mouse_x;
  event.data.mouse.y = window->mouse_y;
  event.data.mouse.modifiers = window->modifiers;
  PushEvent(window, &event);
}

static void PointerAxis(void *data, struct wl_pointer *pointer, u32 time,
                        u32 axis, wl_fixed_t value) {
  (void)pointer;
  (void)time;
  platform_window *window = (platform_window *)data;

  platform_event event = {0};
  event.type = EVENT_MOUSE_SCROLL;
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    event.data.scroll.dy = -wl_fixed_to_double(value) / 10.0f;
  } else {
    event.data.scroll.dx = wl_fixed_to_double(value) / 10.0f;
  }
  PushEvent(window, &event);
}

static void PointerFrame(void *data, struct wl_pointer *pointer) {
  (void)data;
  (void)pointer;
}

static void PointerAxisSource(void *data, struct wl_pointer *pointer,
                              u32 source) {
  (void)data;
  (void)pointer;
  (void)source;
}

static void PointerAxisStop(void *data, struct wl_pointer *pointer, u32 time,
                            u32 axis) {
  (void)data;
  (void)pointer;
  (void)time;
  (void)axis;
}

static void PointerAxisDiscrete(void *data, struct wl_pointer *pointer,
                                u32 axis, i32 discrete) {
  (void)data;
  (void)pointer;
  (void)axis;
  (void)discrete;
}

static void PointerAxisValue120(void *data, struct wl_pointer *pointer,
                                u32 axis, i32 value120) {
  (void)data;
  (void)pointer;
  (void)axis;
  (void)value120;
}

static void PointerAxisRelativeDirection(void *data, struct wl_pointer *pointer,
                                         u32 axis, u32 direction) {
  (void)data;
  (void)pointer;
  (void)axis;
  (void)direction;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = PointerEnter,
    .leave = PointerLeave,
    .motion = PointerMotion,
    .button = PointerButton,
    .axis = PointerAxis,
    .frame = PointerFrame,
    .axis_source = PointerAxisSource,
    .axis_stop = PointerAxisStop,
    .axis_discrete = PointerAxisDiscrete,
    .axis_value120 = PointerAxisValue120,
    .axis_relative_direction = PointerAxisRelativeDirection,
};

/* ===== Clipboard Callbacks ===== */

static void DataSourceTarget(void *data, struct wl_data_source *source,
                             const char *mime_type) {
  (void)data;
  (void)source;
  (void)mime_type;
}

static void DataSourceSend(void *data, struct wl_data_source *source,
                           const char *mime_type, i32 fd) {
  (void)data;
  (void)source;
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
  /* Note: We don't free clipboard_content here because we might want to keep it
   * for internal use, though technically we lost ownership. For now, we'll keep it
   * until next copy or shutdown. */
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

static const struct wl_data_offer_listener data_offer_listener = {
    .offer = DataOfferOffer,
    .source_actions = DataOfferSourceActions,
    .action = DataOfferAction,
};

static void DataDeviceDataOffer(void *data, struct wl_data_device *device,
                                struct wl_data_offer *offer) {
  (void)data;
  (void)device;
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
  (void)data;
  (void)device;
  if (g_platform.selection_offer) {
    wl_data_offer_destroy(g_platform.selection_offer);
  }
  g_platform.selection_offer = offer;
}

static const struct wl_data_device_listener data_device_listener = {
    .data_offer = DataDeviceDataOffer,
    .enter = DataDeviceEnter,
    .leave = DataDeviceLeave,
    .motion = DataDeviceMotion,
    .drop = DataDeviceDrop,
    .selection = DataDeviceSelection,
};


/* ===== Seat Callbacks ===== */

static platform_window *g_current_window = NULL;

static void SeatCapabilities(void *data, struct wl_seat *seat, u32 caps) {
  (void)data;

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !g_platform.keyboard) {
    g_platform.keyboard = wl_seat_get_keyboard(seat);
    if (g_current_window) {
      wl_keyboard_add_listener(g_platform.keyboard, &keyboard_listener,
                               g_current_window);
    }
  }

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !g_platform.pointer) {
    g_platform.pointer = wl_seat_get_pointer(seat);
    if (g_current_window) {
      wl_pointer_add_listener(g_platform.pointer, &pointer_listener,
                              g_current_window);
    }
  }
}

static void SeatName(void *data, struct wl_seat *seat, const char *name) {
  (void)data;
  (void)seat;
  (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = SeatCapabilities,
    .name = SeatName,
};

/* ===== Registry Callbacks ===== */

static void RegistryGlobal(void *data, struct wl_registry *registry, u32 name,
                           const char *interface, u32 version) {
  (void)data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    g_platform.compositor = wl_registry_bind(
        registry, name, &wl_compositor_interface, Min(version, 4));
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    g_platform.shm =
        wl_registry_bind(registry, name, &wl_shm_interface, Min(version, 1));
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    g_platform.xdg_wm_base = wl_registry_bind(
        registry, name, &xdg_wm_base_interface, Min(version, 2));
    xdg_wm_base_add_listener(g_platform.xdg_wm_base, &xdg_wm_base_listener,
                             NULL);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    g_platform.seat =
        wl_registry_bind(registry, name, &wl_seat_interface, Min(version, 5));
    wl_seat_add_listener(g_platform.seat, &seat_listener, NULL);
  } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) ==
             0) {
    g_platform.decoration_manager = wl_registry_bind(
        registry, name, &zxdg_decoration_manager_v1_interface, Min(version, 1));
  } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
    g_platform.data_device_manager = wl_registry_bind(
        registry, name, &wl_data_device_manager_interface, Min(version, 3));
  }
}

static void RegistryGlobalRemove(void *data, struct wl_registry *registry,
                                 u32 name) {
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = RegistryGlobal,
    .global_remove = RegistryGlobalRemove,
};

/* ===== Key Code Mapping ===== */

static key_code LinuxKeyToKeyCode(u32 key) {
  /* Linux evdev key codes */
  switch (key) {
  case 1:
    return KEY_ESCAPE;
  case 28:
    return KEY_RETURN;
  case 15:
    return KEY_TAB;
  case 14:
    return KEY_BACKSPACE;
  case 111:
    return KEY_DELETE;
  case 103:
    return KEY_UP;
  case 108:
    return KEY_DOWN;
  case 105:
    return KEY_LEFT;
  case 106:
    return KEY_RIGHT;
  case 102:
    return KEY_HOME;
  case 107:
    return KEY_END;
  case 104:
    return KEY_PAGE_UP;
  case 109:
    return KEY_PAGE_DOWN;
  case 57:
    return KEY_SPACE;
  case 41:
    return KEY_GRAVE;

  /* Letters */
  case 30:
    return KEY_A;
  case 48:
    return KEY_B;
  case 46:
    return KEY_C;
  case 32:
    return KEY_D;
  case 18:
    return KEY_E;
  case 33:
    return KEY_F;
  case 34:
    return KEY_G;
  case 35:
    return KEY_H;
  case 23:
    return KEY_I;
  case 36:
    return KEY_J;
  case 37:
    return KEY_K;
  case 38:
    return KEY_L;
  case 50:
    return KEY_M;
  case 49:
    return KEY_N;
  case 24:
    return KEY_O;
  case 25:
    return KEY_P;
  case 16:
    return KEY_Q;
  case 19:
    return KEY_R;
  case 31:
    return KEY_S;
  case 20:
    return KEY_T;
  case 22:
    return KEY_U;
  case 47:
    return KEY_V;
  case 17:
    return KEY_W;
  case 45:
    return KEY_X;
  case 21:
    return KEY_Y;
  case 44:
    return KEY_Z;

  /* Numbers */
  case 11:
    return KEY_0;
  case 2:
    return KEY_1;
  case 3:
    return KEY_2;
  case 4:
    return KEY_3;
  case 5:
    return KEY_4;
  case 6:
    return KEY_5;
  case 7:
    return KEY_6;
  case 8:
    return KEY_7;
  case 9:
    return KEY_8;
  case 10:
    return KEY_9;

  /* Function keys */
  case 59:
    return KEY_F1;
  case 60:
    return KEY_F2;
  case 61:
    return KEY_F3;
  case 62:
    return KEY_F4;
  case 63:
    return KEY_F5;
  case 64:
    return KEY_F6;
  case 65:
    return KEY_F7;
  case 66:
    return KEY_F8;
  case 67:
    return KEY_F9;
  case 68:
    return KEY_F10;
  case 87:
    return KEY_F11;
  case 88:
    return KEY_F12;

  /* Punctuation */
  case 12:
    return KEY_MINUS;
  case 13:
    return KEY_EQUALS;
  case 26:
    return KEY_LEFTBRACKET;
  case 27:
    return KEY_RIGHTBRACKET;
  case 43:
    return KEY_BACKSLASH;
  case 39:
    return KEY_SEMICOLON;
  case 40:
    return KEY_APOSTROPHE;
  case 51:
    return KEY_COMMA;
  case 52:
    return KEY_PERIOD;
  case 53:
    return KEY_SLASH;

  default:
    return KEY_UNKNOWN;
  }
}

/* ===== Platform Init/Shutdown ===== */

b32 Platform_Init(void) {
  if (g_platform.initialized)
    return true;

  g_platform.display = wl_display_connect(NULL);
  if (!g_platform.display) {
    fprintf(stderr, "Failed to connect to Wayland display\n");
    return false;
  }

  g_platform.registry = wl_display_get_registry(g_platform.display);
  wl_registry_add_listener(g_platform.registry, &registry_listener, NULL);

  wl_display_roundtrip(g_platform.display);

  if (g_platform.seat && g_platform.data_device_manager) {
    g_platform.data_device = wl_data_device_manager_get_data_device(
        g_platform.data_device_manager, g_platform.seat);
    wl_data_device_add_listener(g_platform.data_device, &data_device_listener,
                                NULL);
  }

  if (!g_platform.compositor || !g_platform.xdg_wm_base || !g_platform.shm) {
    fprintf(stderr, "Missing required Wayland interfaces\n");
    return false;
  }

  g_platform.initialized = true;
  return true;
}

void Platform_Shutdown(void) {
  if (!g_platform.initialized)
    return;

  if (g_platform.keyboard)
    wl_keyboard_destroy(g_platform.keyboard);
  if (g_platform.pointer)
    wl_pointer_destroy(g_platform.pointer);
  if (g_platform.seat)
    wl_seat_destroy(g_platform.seat);
  if (g_platform.xdg_wm_base)
    xdg_wm_base_destroy(g_platform.xdg_wm_base);
  if (g_platform.decoration_manager)
    zxdg_decoration_manager_v1_destroy(g_platform.decoration_manager);
  if (g_platform.shm)
    wl_shm_destroy(g_platform.shm);
  if (g_platform.compositor)
    wl_compositor_destroy(g_platform.compositor);
  if (g_platform.registry)
    wl_registry_destroy(g_platform.registry);
  if (g_platform.display)
    wl_display_disconnect(g_platform.display);

  if (g_platform.clipboard_content)
    free(g_platform.clipboard_content);
  if (g_platform.clipboard_source)
    wl_data_source_destroy(g_platform.clipboard_source);
  if (g_platform.selection_offer)
    wl_data_offer_destroy(g_platform.selection_offer);
  if (g_platform.data_device)
    wl_data_device_destroy(g_platform.data_device);
  if (g_platform.data_device_manager)
    wl_data_device_manager_destroy(g_platform.data_device_manager);

  memset(&g_platform, 0, sizeof(g_platform));
}

/* ===== Window Creation ===== */

static i32 CreateShmBuffer(platform_window *window, i32 index) {
  usize stride = window->width * 4; /* ARGB */
  usize size = stride * window->height;
  window->shm_size = size;

  char name[32];
  snprintf(name, sizeof(name), "/wb-shm-%d-%d", getpid(), index);

  i32 fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    return -1;
  shm_unlink(name);

  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }

  window->shm_data[index] =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (window->shm_data[index] == MAP_FAILED) {
    close(fd);
    return -1;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(g_platform.shm, fd, size);
  window->buffers[index] = wl_shm_pool_create_buffer(
      pool, 0, window->width, window->height, stride, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);

  return 0;
}

platform_window *Platform_CreateWindow(window_config *config) {
  if (!g_platform.initialized) {
    if (!Platform_Init())
      return NULL;
  }

  platform_window *window = calloc(1, sizeof(platform_window));
  if (!window)
    return NULL;

  window->width = config->width;
  window->height = config->height;
  window->resizable = config->resizable;

  window->surface = wl_compositor_create_surface(g_platform.compositor);
  if (!window->surface) {
    free(window);
    return NULL;
  }

  window->xdg_surface =
      xdg_wm_base_get_xdg_surface(g_platform.xdg_wm_base, window->surface);
  xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);

  window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
  xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener,
                            window);

  xdg_toplevel_set_title(window->xdg_toplevel, config->title);
  xdg_toplevel_set_app_id(window->xdg_toplevel, "workbench");

  if (config->resizable) {
    xdg_toplevel_set_min_size(window->xdg_toplevel, 400, 300);
  }

  if (config->maximized) {
    xdg_toplevel_set_maximized(window->xdg_toplevel);
  }

  /* Request server-side decorations if available */
  if (g_platform.decoration_manager) {
    window->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
        g_platform.decoration_manager, window->xdg_toplevel);
    if (window->decoration) {
      zxdg_toplevel_decoration_v1_set_mode(
          window->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
  }

  wl_surface_commit(window->surface);
  wl_display_roundtrip(g_platform.display);

  /* Create double buffers */
  if (CreateShmBuffer(window, 0) < 0 || CreateShmBuffer(window, 1) < 0) {
    Platform_DestroyWindow(window);
    return NULL;
  }
  window->current_buffer = 0;

  /* Clear both buffers to dark background */
  for (i32 b = 0; b < 2; b++) {
    u32 *pixels = (u32 *)window->shm_data[b];
    for (i32 i = 0; i < window->width * window->height; i++) {
      pixels[i] = 0xFF1E1E2E; /* Dark theme background */
    }
  }

  wl_surface_attach(window->surface, window->buffers[0], 0, 0);
  wl_surface_commit(window->surface);

  g_current_window = window;

  /* Attach input listeners if already available */
  if (g_platform.keyboard) {
    wl_keyboard_add_listener(g_platform.keyboard, &keyboard_listener, window);
  }
  if (g_platform.pointer) {
    wl_pointer_add_listener(g_platform.pointer, &pointer_listener, window);
  }

  return window;
}

void Platform_DestroyWindow(platform_window *window) {
  if (!window)
    return;

  DestroyShmBuffers(window);
  if (window->decoration)
    zxdg_toplevel_decoration_v1_destroy(window->decoration);
  if (window->xdg_toplevel)
    xdg_toplevel_destroy(window->xdg_toplevel);
  if (window->xdg_surface)
    xdg_surface_destroy(window->xdg_surface);
  if (window->surface)
    wl_surface_destroy(window->surface);

  if (g_current_window == window) {
    g_current_window = NULL;
  }

  free(window);
}

void Platform_SetWindowTitle(platform_window *window, const char *title) {
  if (window && window->xdg_toplevel) {
    xdg_toplevel_set_title(window->xdg_toplevel, title);
  }
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

/* ===== Event Handling ===== */

#include <poll.h>

b32 Platform_PollEvent(platform_window *window, platform_event *event) {
  if (!window || !event)
    return false;

  /* First, flush any pending outgoing requests */
  while (wl_display_prepare_read(g_platform.display) != 0) {
    wl_display_dispatch_pending(g_platform.display);
  }
  wl_display_flush(g_platform.display);

  /* Poll for new events from the Wayland display fd with a short timeout */
  struct pollfd pfd = {
      .fd = wl_display_get_fd(g_platform.display),
      .events = POLLIN,
  };

  if (poll(&pfd, 1, 0) > 0) {
    /* New events available, read and dispatch them */
    wl_display_read_events(g_platform.display);
    wl_display_dispatch_pending(g_platform.display);
  } else {
    /* No new events, cancel the read */
    wl_display_cancel_read(g_platform.display);
  }

  if (window->event_read != window->event_write) {
    *event = window->events[window->event_read];
    window->event_read = (window->event_read + 1) % ArrayCount(window->events);
    return true;
  }

  return false;
}

void Platform_WaitEvents(platform_window *window) {
  (void)window;
  wl_display_dispatch(g_platform.display);
}

/* ===== File System ===== */

b32 Platform_ListDirectory(const char *path, directory_listing *listing,
                           memory_arena *arena) {
  DIR *dir = opendir(path);
  if (!dir)
    return false;

  listing->count = 0;
  listing->capacity = 2048;
  listing->entries = ArenaPushArray(arena, file_info, listing->capacity);

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0)
      continue;

    if (listing->count >= listing->capacity)
      break;

    file_info *info = &listing->entries[listing->count];
    strncpy(info->name, entry->d_name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode))
        info->type = FILE_TYPE_DIRECTORY;
      else if (S_ISLNK(st.st_mode))
        info->type = FILE_TYPE_SYMLINK;
      else
        info->type = FILE_TYPE_FILE;
      info->size = st.st_size;
      info->modified_time = st.st_mtime;
    }

    listing->count++;
  }

  closedir(dir);
  return true;
}

void Platform_FreeDirectoryListing(directory_listing *listing) {
  (void)listing;
  /* Arena-allocated, no free needed */
}

u8 *Platform_ReadEntireFile(const char *path, usize *out_size,
                            memory_arena *arena) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  usize size = ftell(file);
  fseek(file, 0, SEEK_SET);

  u8 *data = ArenaPushArray(arena, u8, size + 1);
  fread(data, 1, size, file);
  data[size] = '\0';

  fclose(file);

  if (out_size)
    *out_size = size;
  return data;
}

b32 Platform_GetFileInfo(const char *path, file_info *info) {
  struct stat st;
  if (stat(path, &st) != 0)
    return false;

  const char *name = strrchr(path, '/');
  name = name ? name + 1 : path;
  strncpy(info->name, name, sizeof(info->name) - 1);

  if (S_ISDIR(st.st_mode))
    info->type = FILE_TYPE_DIRECTORY;
  else if (S_ISLNK(st.st_mode))
    info->type = FILE_TYPE_SYMLINK;
  else
    info->type = FILE_TYPE_FILE;

  info->size = st.st_size;
  info->modified_time = st.st_mtime;

  return true;
}

b32 Platform_FileExists(const char *path) { return access(path, F_OK) == 0; }

b32 Platform_IsDirectory(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  return S_ISDIR(st.st_mode);
}

/* ===== Time ===== */

u64 Platform_GetTimeMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000 + (u64)ts.tv_nsec / 1000000;
}

/* ===== File System Extras ===== */

void Platform_OpenFile(const char *path) {
  char command[2048];
  /* Use xdg-open to open file with default application */
  /* Redirect output to silence spurious messages */
  snprintf(command, sizeof(command), "xdg-open \"%s\" > /dev/null 2>&1 &", path);
  int result = system(command);
  (void)result;
}

void Platform_SleepMs(u32 ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

/* ===== Clipboard (stub for now) ===== */

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
  /* If we already have a source, we could reuse it or destroy it? 
     Usually new copy = new source */
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

/* ===== Process (stub for now) ===== */

struct platform_process {
  i32 dummy;
};

platform_process *Platform_SpawnProcess(const char *command,
                                        const char *working_dir) {
  (void)command;
  (void)working_dir;
  /* TODO: Implement using fork/exec with pipes */
  return NULL;
}

b32 Platform_ProcessIsRunning(platform_process *process) {
  (void)process;
  return false;
}

i32 Platform_ProcessRead(platform_process *process, char *buffer,
                         usize buffer_size) {
  (void)process;
  (void)buffer;
  (void)buffer_size;
  return 0;
}

i32 Platform_ProcessWrite(platform_process *process, const char *data,
                          usize size) {
  (void)process;
  (void)data;
  (void)size;
  return 0;
}

void Platform_ProcessKill(platform_process *process) { (void)process; }

void Platform_ProcessFree(platform_process *process) { (void)process; }

/* ===== Framebuffer Access ===== */

void *Platform_GetFramebuffer(platform_window *window) {
  if (!window)
    return NULL;
  /* Return back buffer (the one NOT currently displayed) */
  return window->shm_data[window->current_buffer];
}

void Platform_PresentFrame(platform_window *window) {
  if (!window || !window->surface)
    return;

  i32 buf_idx = window->current_buffer;
  if (!window->buffers[buf_idx])
    return;

  wl_surface_attach(window->surface, window->buffers[buf_idx], 0, 0);
  wl_surface_damage_buffer(window->surface, 0, 0, window->width,
                           window->height);
  wl_surface_commit(window->surface);

  /* Swap to other buffer for next frame */
  window->current_buffer = 1 - window->current_buffer;
}
