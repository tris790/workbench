/*
 * linux_events.c - Linux/Wayland event handling
 */

#include "linux_internal.h"
#include <poll.h>

extern platform_window *g_current_window;

/* ===== Event Queue Helpers ===== */

void PushEvent(platform_window *window, platform_event *event) {
  usize next = (window->event_write + 1) % ArrayCount(window->events);
  if (next != window->event_read) {
    window->events[window->event_write] = *event;
    window->event_write = next;
  }
}

/* ===== Key Code Mapping ===== */

static key_code LinuxKeyToKeyCode(u32 key) {
  /* Linux evdev key codes */
  switch (key) {
  case 1: return KEY_ESCAPE;
  case 28: return KEY_RETURN;
  case 15: return KEY_TAB;
  case 14: return KEY_BACKSPACE;
  case 111: return KEY_DELETE;
  case 103: return KEY_UP;
  case 108: return KEY_DOWN;
  case 105: return KEY_LEFT;
  case 106: return KEY_RIGHT;
  case 102: return KEY_HOME;
  case 107: return KEY_END;
  case 104: return KEY_PAGE_UP;
  case 109: return KEY_PAGE_DOWN;
  case 57: return KEY_SPACE;
  case 41: return KEY_GRAVE;

  /* Letters */
  case 30: return KEY_A;
  case 48: return KEY_B;
  case 46: return KEY_C;
  case 32: return KEY_D;
  case 18: return KEY_E;
  case 33: return KEY_F;
  case 34: return KEY_G;
  case 35: return KEY_H;
  case 23: return KEY_I;
  case 36: return KEY_J;
  case 37: return KEY_K;
  case 38: return KEY_L;
  case 50: return KEY_M;
  case 49: return KEY_N;
  case 24: return KEY_O;
  case 25: return KEY_P;
  case 16: return KEY_Q;
  case 19: return KEY_R;
  case 31: return KEY_S;
  case 20: return KEY_T;
  case 22: return KEY_U;
  case 47: return KEY_V;
  case 17: return KEY_W;
  case 45: return KEY_X;
  case 21: return KEY_Y;
  case 44: return KEY_Z;

  /* Numbers */
  case 11: return KEY_0;
  case 2: return KEY_1;
  case 3: return KEY_2;
  case 4: return KEY_3;
  case 5: return KEY_4;
  case 6: return KEY_5;
  case 7: return KEY_6;
  case 8: return KEY_7;
  case 9: return KEY_8;
  case 10: return KEY_9;

  /* Function keys */
  case 59: return KEY_F1;
  case 60: return KEY_F2;
  case 61: return KEY_F3;
  case 62: return KEY_F4;
  case 63: return KEY_F5;
  case 64: return KEY_F6;
  case 65: return KEY_F7;
  case 66: return KEY_F8;
  case 67: return KEY_F9;
  case 68: return KEY_F10;
  case 87: return KEY_F11;
  case 88: return KEY_F12;

  /* Punctuation */
  case 12: return KEY_MINUS;
  case 13: return KEY_EQUALS;
  case 26: return KEY_LEFTBRACKET;
  case 27: return KEY_RIGHTBRACKET;
  case 43: return KEY_BACKSLASH;
  case 39: return KEY_SEMICOLON;
  case 40: return KEY_APOSTROPHE;
  case 51: return KEY_COMMA;
  case 52: return KEY_PERIOD;
  case 53: return KEY_SLASH;

  default: return KEY_UNKNOWN;
  }
}

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
  case KEY_SPACE: return ' ';
  case KEY_GRAVE: return shift ? '~' : '`';
  case KEY_MINUS: return shift ? '_' : '-';
  case KEY_EQUALS: return shift ? '+' : '=';
  case KEY_LEFTBRACKET: return shift ? '{' : '[';
  case KEY_RIGHTBRACKET: return shift ? '}' : ']';
  case KEY_BACKSLASH: return shift ? '|' : '\\';
  case KEY_SEMICOLON: return shift ? ':' : ';';
  case KEY_APOSTROPHE: return shift ? '"' : '\'';
  case KEY_COMMA: return shift ? '<' : ',';
  case KEY_PERIOD: return shift ? '>' : '.';
  case KEY_SLASH: return shift ? '?' : '/';
  default: return 0;
  }
}

/* ===== Keyboard Callbacks ===== */

static void KeyboardKeymap(void *data, struct wl_keyboard *keyboard, u32 format,
                           i32 fd, u32 size) {
  (void)data; (void)keyboard; (void)format; (void)size;
  close(fd);
}

static void KeyboardEnter(void *data, struct wl_keyboard *keyboard, u32 serial,
                          struct wl_surface *surface, struct wl_array *keys) {
  (void)keyboard; (void)surface; (void)keys;
  g_platform.last_serial = serial;
  platform_window *window = (platform_window *)data;
  
  platform_event event = {0};
  event.type = EVENT_WINDOW_FOCUS;
  PushEvent(window, &event);
}

static void KeyboardLeave(void *data, struct wl_keyboard *keyboard, u32 serial,
                          struct wl_surface *surface) {
  (void)keyboard; (void)serial; (void)surface;
  platform_window *window = (platform_window *)data;
  
  platform_event event = {0};
  event.type = EVENT_WINDOW_UNFOCUS;
  PushEvent(window, &event);
}

static void KeyboardKey(void *data, struct wl_keyboard *keyboard, u32 serial,
                        u32 time, u32 key, u32 state) {
  (void)keyboard; (void)time;
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
  (void)keyboard; (void)serial; (void)mods_latched; (void)mods_locked; (void)group;
  platform_window *window = (platform_window *)data;

  window->modifiers = 0;
  if (mods_depressed & 1) window->modifiers |= MOD_SHIFT;
  if (mods_depressed & 4) window->modifiers |= MOD_CTRL;
  if (mods_depressed & 8) window->modifiers |= MOD_ALT;
  if (mods_depressed & 64) window->modifiers |= MOD_SUPER;
}

static void KeyboardRepeatInfo(void *data, struct wl_keyboard *keyboard,
                               i32 rate, i32 delay) {
  (void)data; (void)keyboard; (void)rate; (void)delay;
}

const struct wl_keyboard_listener keyboard_listener = {
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
  (void)pointer; (void)surface;
  g_platform.last_serial = serial;
  platform_window *window = (platform_window *)data;
  window->mouse_x = wl_fixed_to_int(sx);
  window->mouse_y = wl_fixed_to_int(sy);
}

static void PointerLeave(void *data, struct wl_pointer *pointer, u32 serial,
                         struct wl_surface *surface) {
  (void)data; (void)pointer; (void)serial; (void)surface;
}

static void PointerMotion(void *data, struct wl_pointer *pointer, u32 time,
                          wl_fixed_t sx, wl_fixed_t sy) {
  (void)pointer; (void)time;
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
  (void)pointer; (void)time;
  g_platform.last_serial = serial;
  platform_window *window = (platform_window *)data;

  platform_event event = {0};
  event.type = (state == WL_POINTER_BUTTON_STATE_PRESSED)
                   ? EVENT_MOUSE_BUTTON_DOWN
                   : EVENT_MOUSE_BUTTON_UP;

  /* Linux button codes: 272=left, 273=right, 274=middle, 275=side, 276=extra */
  if (button == 272) event.data.mouse.button = MOUSE_LEFT;
  else if (button == 273) event.data.mouse.button = MOUSE_RIGHT;
  else if (button == 274) event.data.mouse.button = MOUSE_MIDDLE;
  else if (button == 275) event.data.mouse.button = MOUSE_X1;
  else if (button == 276) event.data.mouse.button = MOUSE_X2;

  event.data.mouse.x = window->mouse_x;
  event.data.mouse.y = window->mouse_y;
  event.data.mouse.modifiers = window->modifiers;
  PushEvent(window, &event);
}

static void PointerAxis(void *data, struct wl_pointer *pointer, u32 time,
                        u32 axis, wl_fixed_t value) {
  (void)pointer; (void)time;
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
  (void)data; (void)pointer;
}

static void PointerAxisSource(void *data, struct wl_pointer *pointer,
                              u32 source) {
  (void)data; (void)pointer; (void)source;
}

static void PointerAxisStop(void *data, struct wl_pointer *pointer, u32 time,
                            u32 axis) {
  (void)data; (void)pointer; (void)time; (void)axis;
}

static void PointerAxisDiscrete(void *data, struct wl_pointer *pointer,
                                u32 axis, i32 discrete) {
  (void)data; (void)pointer; (void)axis; (void)discrete;
}

static void PointerAxisValue120(void *data, struct wl_pointer *pointer,
                                u32 axis, i32 value120) {
  (void)data; (void)pointer; (void)axis; (void)value120;
}

static void PointerAxisRelativeDirection(void *data, struct wl_pointer *pointer,
                                         u32 axis, u32 direction) {
  (void)data; (void)pointer; (void)axis; (void)direction;
}

const struct wl_pointer_listener pointer_listener = {
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

/* ===== Seat Callbacks ===== */

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
  (void)data; (void)seat; (void)name;
}

const struct wl_seat_listener seat_listener = {
    .capabilities = SeatCapabilities,
    .name = SeatName,
};

/* ===== Event Polling ===== */

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
