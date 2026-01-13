/*
 * platform.h - Platform abstraction layer for Workbench
 *
 * Defines platform-agnostic API for window management, events,
 * file system operations, clipboard, and process spawning.
 * C99, handmade hero style.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "types.h"

/* ===== Forward Declarations ===== */

typedef struct platform_window platform_window;
typedef struct platform_event platform_event;

/* ===== Event Types ===== */

typedef enum {
  EVENT_NONE = 0,
  EVENT_QUIT,
  EVENT_KEY_DOWN,
  EVENT_KEY_UP,
  EVENT_MOUSE_BUTTON_DOWN,
  EVENT_MOUSE_BUTTON_UP,
  EVENT_MOUSE_MOVE,
  EVENT_MOUSE_SCROLL,
  EVENT_WINDOW_RESIZE,
  EVENT_WINDOW_FOCUS,
  EVENT_WINDOW_UNFOCUS,
} event_type;

typedef enum {
  KEY_UNKNOWN = 0,
  KEY_ESCAPE,
  KEY_RETURN,
  KEY_TAB,
  KEY_BACKSPACE,
  KEY_DELETE,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_HOME,
  KEY_END,
  KEY_PAGE_UP,
  KEY_PAGE_DOWN,
  KEY_SPACE,
  KEY_A,
  KEY_B,
  KEY_C,
  KEY_D,
  KEY_E,
  KEY_F,
  KEY_G,
  KEY_H,
  KEY_I,
  KEY_J,
  KEY_K,
  KEY_L,
  KEY_M,
  KEY_N,
  KEY_O,
  KEY_P,
  KEY_Q,
  KEY_R,
  KEY_S,
  KEY_T,
  KEY_U,
  KEY_V,
  KEY_W,
  KEY_X,
  KEY_Y,
  KEY_Z,
  KEY_0,
  KEY_1,
  KEY_2,
  KEY_3,
  KEY_4,
  KEY_5,
  KEY_6,
  KEY_7,
  KEY_8,
  KEY_9,
  KEY_F1,
  KEY_F2,
  KEY_F3,
  KEY_F4,
  KEY_F5,
  KEY_F6,
  KEY_F7,
  KEY_F8,
  KEY_F9,
  KEY_F10,
  KEY_F11,
  KEY_F12,
  KEY_GRAVE, /* ` for debug console */
  KEY_MINUS,
  KEY_EQUALS,
  KEY_LEFTBRACKET,
  KEY_RIGHTBRACKET,
  KEY_BACKSLASH,
  KEY_SEMICOLON,
  KEY_APOSTROPHE,
  KEY_COMMA,
  KEY_PERIOD,
  KEY_SLASH,
  KEY_BROWSER_BACK,
  KEY_BROWSER_FORWARD,
  KEY_LSHIFT,
  KEY_RSHIFT,
  KEY_LCTRL,
  KEY_RCTRL,
  KEY_LALT,
  KEY_RALT,
  KEY_LSUPER,
  KEY_RSUPER,
  KEY_COUNT
} key_code;

typedef enum {
  MOUSE_LEFT = 0,
  MOUSE_MIDDLE,
  MOUSE_RIGHT,
  MOUSE_X1, /* Back */
  MOUSE_X2, /* Forward */
  MOUSE_BUTTON_COUNT
} mouse_button;

/* Key modifier flags */
#define MOD_SHIFT (1 << 0)
#define MOD_CTRL (1 << 1)
#define MOD_ALT (1 << 2)
#define MOD_SUPER (1 << 3)

struct platform_event {
  event_type type;
  union {
    struct {
      key_code key;
      u32 modifiers;
      u32 character; /* UTF-32 codepoint if text input */
    } keyboard;
    struct {
      mouse_button button;
      i32 x, y;
      u32 modifiers;
    } mouse;
    struct {
      f32 dx, dy; /* scroll delta */
    } scroll;
    struct {
      i32 width, height;
    } resize;
  } data;
};

/* ===== File System Types ===== */

typedef enum {
  FILE_TYPE_UNKNOWN = 0,
  FILE_TYPE_FILE,
  FILE_TYPE_DIRECTORY,
  FILE_TYPE_SYMLINK,
} file_type;

typedef struct {
  char name[256];
  file_type type;
  u64 size;
  u64 modified_time; /* Unix timestamp */
} file_info;

typedef struct {
  file_info *entries;
  usize count;
  usize capacity;
} directory_listing;

/* ===== Window API ===== */

typedef struct {
  const char *title;
  i32 width;
  i32 height;
  b32 resizable;
  b32 maximized;
} window_config;

platform_window *Platform_CreateWindow(window_config *config);
void Platform_DestroyWindow(platform_window *window);
void Platform_SetWindowTitle(platform_window *window, const char *title);
void Platform_GetWindowSize(platform_window *window, i32 *width, i32 *height);
b32 Platform_WindowShouldClose(platform_window *window);

/* ===== Event API ===== */

b32 Platform_PollEvent(platform_window *window, platform_event *event);
void Platform_WaitEvents(platform_window *window);

/* ===== File System API ===== */

b32 Platform_ListDirectory(const char *path, directory_listing *listing,
                           memory_arena *arena);
void Platform_FreeDirectoryListing(directory_listing *listing);
u8 *Platform_ReadEntireFile(const char *path, usize *out_size,
                            memory_arena *arena);
b32 Platform_GetFileInfo(const char *path, file_info *info);
b32 Platform_FileExists(const char *path);
b32 Platform_IsDirectory(const char *path);
void Platform_OpenFile(const char *path);
b32 Platform_CreateDirectory(const char *path);
b32 Platform_CreateFile(const char *path);
b32 Platform_Delete(const char *path);
b32 Platform_Rename(const char *old_path, const char *new_path);
b32 Platform_Copy(const char *src, const char *dst);
b32 Platform_GetRealPath(const char *path, char *out_path, usize out_size);

/* ===== Clipboard API ===== */

char *Platform_GetClipboard(char *buffer, usize buffer_size);
b32 Platform_SetClipboard(const char *text);

/* ===== Process API ===== */

typedef struct platform_process platform_process;

platform_process *Platform_SpawnProcess(const char *command,
                                        const char *working_dir);
b32 Platform_ProcessIsRunning(platform_process *process);
i32 Platform_ProcessRead(platform_process *process, char *buffer,
                         usize buffer_size);
i32 Platform_ProcessWrite(platform_process *process, const char *data,
                          usize size);
void Platform_ProcessKill(platform_process *process);
void Platform_ProcessFree(platform_process *process);

/* ===== Time API ===== */

u64 Platform_GetTimeMs(void);
void Platform_SleepMs(u32 ms);

/* ===== Framebuffer Access ===== */

void *Platform_GetFramebuffer(platform_window *window);
void Platform_PresentFrame(platform_window *window);

/* ===== Platform Initialization ===== */

b32 Platform_Init(void);
void Platform_Shutdown(void);

#endif /* PLATFORM_H */
