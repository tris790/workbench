#ifdef _WIN32
/*
 * windows_events.c - Windows event handling
 *
 * Handles keyboard, mouse, and window events via WndProc.
 * C99, handmade hero style.
 */

#include "windows_internal.h"

/* ===== Event Queue Helpers ===== */

void PushEvent(platform_window *window, platform_event *event) {
  if (!window)
    return;
  usize next = (window->event_write + 1) % EVENT_QUEUE_SIZE;
  if (next == window->event_read) {
    /* Queue is full, discard oldest event by advancing read head */
    window->event_read = (window->event_read + 1) % EVENT_QUEUE_SIZE;
  }
  window->events[window->event_write] = *event;
  window->event_write = next;
}

/* ===== Modifier State ===== */

u32 GetModifierState(void) {
  u32 mods = 0;
  if ((GetKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_LSHIFT) & 0x8000) ||
      (GetKeyState(VK_RSHIFT) & 0x8000))
    mods |= MOD_SHIFT;
  if ((GetKeyState(VK_CONTROL) & 0x8000) ||
      (GetKeyState(VK_LCONTROL) & 0x8000) ||
      (GetKeyState(VK_RCONTROL) & 0x8000))
    mods |= MOD_CTRL;
  if ((GetKeyState(VK_MENU) & 0x8000) || (GetKeyState(VK_LMENU) & 0x8000) ||
      (GetKeyState(VK_RMENU) & 0x8000))
    mods |= MOD_ALT;
  if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000))
    mods |= MOD_SUPER;
  return mods;
}

/* ===== Virtual Key to Key Code Mapping ===== */

key_code VirtualKeyToKeyCode(WPARAM vk) {
  switch (vk) {
  case VK_ESCAPE:
    return KEY_ESCAPE;
  case VK_RETURN:
    return KEY_RETURN;
  case VK_TAB:
    return KEY_TAB;
  case VK_BACK:
    return KEY_BACKSPACE;
  case VK_DELETE:
    return KEY_DELETE;
  case VK_UP:
    return KEY_UP;
  case VK_DOWN:
    return KEY_DOWN;
  case VK_LEFT:
    return KEY_LEFT;
  case VK_RIGHT:
    return KEY_RIGHT;
  case VK_HOME:
    return KEY_HOME;
  case VK_END:
    return KEY_END;
  case VK_PRIOR:
    return KEY_PAGE_UP;
  case VK_NEXT:
    return KEY_PAGE_DOWN;
  case VK_SPACE:
    return KEY_SPACE;
  case 'A':
    return KEY_A;
  case 'B':
    return KEY_B;
  case 'C':
    return KEY_C;
  case 'D':
    return KEY_D;
  case 'E':
    return KEY_E;
  case 'F':
    return KEY_F;
  case 'G':
    return KEY_G;
  case 'H':
    return KEY_H;
  case 'I':
    return KEY_I;
  case 'J':
    return KEY_J;
  case 'K':
    return KEY_K;
  case 'L':
    return KEY_L;
  case 'M':
    return KEY_M;
  case 'N':
    return KEY_N;
  case 'O':
    return KEY_O;
  case 'P':
    return KEY_P;
  case 'Q':
    return KEY_Q;
  case 'R':
    return KEY_R;
  case 'S':
    return KEY_S;
  case 'T':
    return KEY_T;
  case 'U':
    return KEY_U;
  case 'V':
    return KEY_V;
  case 'W':
    return KEY_W;
  case 'X':
    return KEY_X;
  case 'Y':
    return KEY_Y;
  case 'Z':
    return KEY_Z;
  case '0':
    return KEY_0;
  case '1':
    return KEY_1;
  case '2':
    return KEY_2;
  case '3':
    return KEY_3;
  case '4':
    return KEY_4;
  case '5':
    return KEY_5;
  case '6':
    return KEY_6;
  case '7':
    return KEY_7;
  case '8':
    return KEY_8;
  case '9':
    return KEY_9;
  case VK_F1:
    return KEY_F1;
  case VK_F2:
    return KEY_F2;
  case VK_F3:
    return KEY_F3;
  case VK_F4:
    return KEY_F4;
  case VK_F5:
    return KEY_F5;
  case VK_F6:
    return KEY_F6;
  case VK_F7:
    return KEY_F7;
  case VK_F8:
    return KEY_F8;
  case VK_F9:
    return KEY_F9;
  case VK_F10:
    return KEY_F10;
  case VK_F11:
    return KEY_F11;
  case VK_F12:
    return KEY_F12;
  case VK_OEM_3:
    return KEY_GRAVE; /* ` ~ */
  case VK_OEM_MINUS:
    return KEY_MINUS;
  case VK_OEM_PLUS:
    return KEY_EQUALS;
  case VK_OEM_4:
    return KEY_LEFTBRACKET; /* [ { */
  case VK_OEM_6:
    return KEY_RIGHTBRACKET; /* ] } */
  case VK_OEM_5:
    return KEY_BACKSLASH; /* \ | */
  case VK_OEM_1:
    return KEY_SEMICOLON; /* ; : */
  case VK_OEM_7:
    return KEY_APOSTROPHE; /* ' " */
  case VK_OEM_COMMA:
    return KEY_COMMA;
  case VK_OEM_PERIOD:
    return KEY_PERIOD;
  case VK_BROWSER_BACK:
    return KEY_BROWSER_BACK;
  case VK_BROWSER_FORWARD:
    return KEY_BROWSER_FORWARD;
  case VK_LSHIFT:
    return KEY_LSHIFT;
  case VK_RSHIFT:
    return KEY_RSHIFT;
  case VK_LCONTROL:
    return KEY_LCTRL;
  case VK_RCONTROL:
    return KEY_RCTRL;
  case VK_LMENU:
    return KEY_LALT;
  case VK_RMENU:
    return KEY_RALT;
  case VK_LWIN:
    return KEY_LSUPER;
  case VK_RWIN:
    return KEY_RSUPER;
  /* Generic mappings if L/R not distinguished by windows in some cases?
     Usually windows sends L/R VKs but let's map generic just in case?
     No, VK_SHIFT is a "virtual" virtual key, usually LSHIFT/RSHIFT are sent. */
  case VK_SHIFT:
    return KEY_LSHIFT; /* fallback */
  case VK_CONTROL:
    return KEY_LCTRL; /* fallback */
  case VK_MENU:
    return KEY_LALT; /* fallback */
  case VK_OEM_2:
    return KEY_SLASH; /* / ? */
  default:
    return KEY_UNKNOWN;
  }
}

/* ===== Window Procedure ===== */

static void HandleResize(platform_window *window, i32 width, i32 height) {
  if (width <= 0 || height <= 0)
    return;
  if (window->width == width && window->height == height)
    return;

  /* Recreate framebuffer with new size */
  DestroyFramebuffer(window);
  window->width = width;
  window->height = height;
  CreateFramebuffer(window);

  platform_event event = {0};
  event.type = EVENT_WINDOW_RESIZE;
  event.data.resize.width = width;
  event.data.resize.height = height;
  PushEvent(window, &event);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  platform_window *window =
      (platform_window *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

  switch (msg) {
  case WM_CLOSE: {
    if (window) {
      window->should_close = true;
      platform_event event = {0};
      event.type = EVENT_QUIT;
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_SIZE: {
    if (window) {
      i32 width = LOWORD(lParam);
      i32 height = HIWORD(lParam);
      HandleResize(window, width, height);
    }
    return 0;
  }

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    if (window) {
      window->last_vk = wParam;

      u32 mods = GetModifierState();
      /* BRUTE FORCE ALT FIX */
      if ((GetAsyncKeyState(VK_MENU) & 0x8000) || (msg == WM_SYSKEYDOWN) ||
          ((lParam >> 29) & 1)) {
        mods |= 4; /* MOD_ALT is 4 */
      }
      if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        mods |= 2; /* MOD_CTRL is 2 */
      if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        mods |= 1; /* MOD_SHIFT is 1 */

      /* Check if this key will produce a printable character via WM_CHAR.
       * We use MapVirtualKey instead of ToUnicode to avoid modifying the
       * kernel keyboard state (which can break dead keys and cause infinite
       * loops with tools like PowerToys Quick Accent).
       */
      UINT ch = MapVirtualKeyW((UINT)wParam, 2 /* MAPVK_VK_TO_CHAR */);
      bool is_printable = ((ch & 0x7FFFFFFF) >= 32 && (ch & 0x7FFFFFFF) != 127);

      /* We send the event now if:
       * 1. It's NOT a printable character (e.g. Esc, Arrows, Return, etc.)
       * 2. A modifier like Ctrl/Alt is held (which might change the character
       * to a control code)
       */
      if (!is_printable || (mods & (MOD_CTRL | MOD_ALT | MOD_SUPER))) {
        platform_event event = {0};
        event.type = EVENT_KEY_DOWN;
        event.data.keyboard.key = VirtualKeyToKeyCode(wParam);
        event.data.keyboard.modifiers = mods;
        event.data.keyboard.character = 0;
        PushEvent(window, &event);
      }
    }

    /* IMPORTANT: Return 0 for WM_SYSKEYDOWN to prevent default system handling
       (like sending focus to menu bar) unless we want system behavior. For
       Alt+Left/Right, we want to handle it. */
    if (msg == WM_SYSKEYDOWN)
      return 0;
    break;
  }

  case WM_KEYUP:
  case WM_SYSKEYUP: {
    if (window) {
      platform_event event = {0};
      event.type = EVENT_KEY_UP;
      event.data.keyboard.key = VirtualKeyToKeyCode(wParam);
      event.data.keyboard.modifiers = GetModifierState();
      PushEvent(window, &event);
    }
    break;
  }

  case WM_CHAR: {
    if (window && wParam >= 32) { /* Ignore control characters */
      platform_event event = {0};
      event.type = EVENT_KEY_DOWN;
      /* Extract scan code from lParam and convert to virtual key.
       * This is more reliable than using last_vk when typing fast,
       * because last_vk can be overwritten by subsequent WM_KEYDOWN
       * messages before their corresponding WM_CHAR is processed. */
      UINT scan_code = (lParam >> 16) & 0xFF;
      UINT vk = MapVirtualKeyW(scan_code, MAPVK_VSC_TO_VK);
      if (vk == 0) {
        /* Fallback to last_vk if MapVirtualKey fails */
        vk = (UINT)window->last_vk;
      }
      event.data.keyboard.key = VirtualKeyToKeyCode(vk);
      event.data.keyboard.modifiers = GetModifierState();
      event.data.keyboard.character = (u32)wParam;
      PushEvent(window, &event);

      /* NOTE: We no longer send an immediate EVENT_KEY_UP here.
       * The physical key release will be caught by WM_KEYUP, ensuring
       * correct key-state tracking in the application. */
    }
    return 0;
  }

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_XBUTTONDOWN: {
    if (window) {
      SetCapture(hwnd);
      platform_event event = {0};
      event.type = EVENT_MOUSE_BUTTON_DOWN;
      event.data.mouse.x = (i16)LOWORD(lParam);
      event.data.mouse.y = (i16)HIWORD(lParam);
      event.data.mouse.modifiers = GetModifierState();
      if (msg == WM_LBUTTONDOWN)
        event.data.mouse.button = MOUSE_LEFT;
      else if (msg == WM_RBUTTONDOWN)
        event.data.mouse.button = MOUSE_RIGHT;
      else if (msg == WM_MBUTTONDOWN)
        event.data.mouse.button = MOUSE_MIDDLE;
      else if (msg == WM_XBUTTONDOWN) {
        event.data.mouse.button =
            (HIWORD(wParam) == XBUTTON1) ? MOUSE_X1 : MOUSE_X2;
      }
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
  case WM_XBUTTONUP: {
    if (window) {
      ReleaseCapture();
      platform_event event = {0};
      event.type = EVENT_MOUSE_BUTTON_UP;
      event.data.mouse.x = (i16)LOWORD(lParam);
      event.data.mouse.y = (i16)HIWORD(lParam);
      event.data.mouse.modifiers = GetModifierState();
      if (msg == WM_LBUTTONUP)
        event.data.mouse.button = MOUSE_LEFT;
      else if (msg == WM_RBUTTONUP)
        event.data.mouse.button = MOUSE_RIGHT;
      else if (msg == WM_MBUTTONUP)
        event.data.mouse.button = MOUSE_MIDDLE;
      else if (msg == WM_XBUTTONUP) {
        event.data.mouse.button =
            (HIWORD(wParam) == XBUTTON1) ? MOUSE_X1 : MOUSE_X2;
      }
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_MOUSEMOVE: {
    if (window) {
      window->mouse_x = (i16)LOWORD(lParam);
      window->mouse_y = (i16)HIWORD(lParam);
      platform_event event = {0};
      event.type = EVENT_MOUSE_MOVE;
      event.data.mouse.x = window->mouse_x;
      event.data.mouse.y = window->mouse_y;
      event.data.mouse.modifiers = GetModifierState();
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_MOUSEWHEEL: {
    if (window) {
      platform_event event = {0};
      event.type = EVENT_MOUSE_SCROLL;
      event.data.scroll.dx = 0.0f;
      event.data.scroll.dy = (f32)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_MOUSEHWHEEL: {
    if (window) {
      platform_event event = {0};
      event.type = EVENT_MOUSE_SCROLL;
      event.data.scroll.dx = (f32)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
      event.data.scroll.dy = 0.0f;
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_SETFOCUS: {
    if (window) {
      platform_event event = {0};
      event.type = EVENT_WINDOW_FOCUS;
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_KILLFOCUS: {
    if (window) {
      platform_event event = {0};
      event.type = EVENT_WINDOW_UNFOCUS;
      PushEvent(window, &event);
    }
    return 0;
  }

  case WM_APPCOMMAND: {
    if (window) {
      UINT cmd = GET_APPCOMMAND_LPARAM(lParam);
      if (cmd == APPCOMMAND_BROWSER_BACKWARD) {
        platform_event event = {0};
        event.type = EVENT_KEY_DOWN;
        event.data.keyboard.key = KEY_BROWSER_BACK;
        event.data.keyboard.modifiers = GetModifierState();
        PushEvent(window, &event);

        event.type = EVENT_KEY_UP;
        PushEvent(window, &event);
        return TRUE;
      } else if (cmd == APPCOMMAND_BROWSER_FORWARD) {
        platform_event event = {0};
        event.type = EVENT_KEY_DOWN;
        event.data.keyboard.key = KEY_BROWSER_FORWARD;
        event.data.keyboard.modifiers = GetModifierState();
        PushEvent(window, &event);

        event.type = EVENT_KEY_UP;
        PushEvent(window, &event);
        return TRUE;
      }
    }
    break;
  }

  case WM_PAINT: {
    if (window && window->framebuffer) {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      BitBlt(hdc, 0, 0, window->width, window->height, window->mem_dc, 0, 0,
             SRCCOPY);
      EndPaint(hwnd, &ps);
    }
    return 0;
  }

  default:
    break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ===== Event Polling ===== */

b32 Platform_PollEvent(platform_window *window, platform_event *event) {
  if (!window)
    return false;

  /* First, process any pending Windows messages */
  MSG msg;
  while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      window->should_close = true;
      platform_event quit_event = {0};
      quit_event.type = EVENT_QUIT;
      PushEvent(window, &quit_event);
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  /* Then, return events from our queue */
  if (window->event_read != window->event_write) {
    *event = window->events[window->event_read];
    window->event_read = (window->event_read + 1) % EVENT_QUEUE_SIZE;
    return true;
  }

  return false;
}

void Platform_WaitEvents(platform_window *window) {
  if (!window)
    return;
  WaitMessage();
}
#endif /* _WIN32 */
