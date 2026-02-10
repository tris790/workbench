/*
 * platform.h - Platform abstraction layer for Workbench
 *
 * Defines platform-agnostic API for window management, events,
 * file system operations, clipboard, and process spawning.
 * C99, handmade hero style.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "../core/types.h"

/* ===== Forward Declarations ===== */

typedef struct platform_window platform_window;
typedef struct platform_event platform_event;

/* ===== Event Types ===== */

typedef enum {
  WB_EVENT_NONE = 0,
  WB_EVENT_QUIT,
  WB_EVENT_KEY_DOWN,
  WB_EVENT_KEY_UP,
  WB_EVENT_MOUSE_BUTTON_DOWN,
  WB_EVENT_MOUSE_BUTTON_UP,
  WB_EVENT_MOUSE_MOVE,
  WB_EVENT_MOUSE_SCROLL,
  WB_EVENT_WINDOW_RESIZE,
  WB_EVENT_WINDOW_FOCUS,
  WB_EVENT_WINDOW_UNFOCUS,
} event_type;

typedef enum {
  WB_KEY_UNKNOWN = 0,
  WB_KEY_ESCAPE,
  WB_KEY_RETURN,
  WB_KEY_TAB,
  WB_KEY_BACKSPACE,
  WB_KEY_DELETE,
  WB_KEY_UP,
  WB_KEY_DOWN,
  WB_KEY_LEFT,
  WB_KEY_RIGHT,
  WB_KEY_HOME,
  WB_KEY_END,
  WB_KEY_PAGE_UP,
  WB_KEY_PAGE_DOWN,
  WB_KEY_SPACE,
  WB_KEY_A,
  WB_KEY_B,
  WB_KEY_C,
  WB_KEY_D,
  WB_KEY_E,
  WB_KEY_F,
  WB_KEY_G,
  WB_KEY_H,
  WB_KEY_I,
  WB_KEY_J,
  WB_KEY_K,
  WB_KEY_L,
  WB_KEY_M,
  WB_KEY_N,
  WB_KEY_O,
  WB_KEY_P,
  WB_KEY_Q,
  WB_KEY_R,
  WB_KEY_S,
  WB_KEY_T,
  WB_KEY_U,
  WB_KEY_V,
  WB_KEY_W,
  WB_KEY_X,
  WB_KEY_Y,
  WB_KEY_Z,
  WB_KEY_0,
  WB_KEY_1,
  WB_KEY_2,
  WB_KEY_3,
  WB_KEY_4,
  WB_KEY_5,
  WB_KEY_6,
  WB_KEY_7,
  WB_KEY_8,
  WB_KEY_9,
  WB_KEY_F1,
  WB_KEY_F2,
  WB_KEY_F3,
  WB_KEY_F4,
  WB_KEY_F5,
  WB_KEY_F6,
  WB_KEY_F7,
  WB_KEY_F8,
  WB_KEY_F9,
  WB_KEY_F10,
  WB_KEY_F11,
  WB_KEY_F12,
  WB_KEY_GRAVE, /* ` for debug console */
  WB_KEY_MINUS,
  WB_KEY_EQUALS,
  WB_KEY_LEFTBRACKET,
  WB_KEY_RIGHTBRACKET,
  WB_KEY_BACKSLASH,
  WB_KEY_SEMICOLON,
  WB_KEY_APOSTROPHE,
  WB_KEY_COMMA,
  WB_KEY_PERIOD,
  WB_KEY_SLASH,
  WB_KEY_BROWSER_BACK,
  WB_KEY_BROWSER_FORWARD,
  WB_KEY_LSHIFT,
  WB_KEY_RSHIFT,
  WB_KEY_LCTRL,
  WB_KEY_RCTRL,
  WB_KEY_LALT,
  WB_KEY_RALT,
  WB_KEY_LSUPER,
  WB_KEY_RSUPER,
  WB_KEY_COUNT
} key_code;

typedef enum {
  WB_MOUSE_LEFT = 0,
  WB_MOUSE_MIDDLE,
  WB_MOUSE_RIGHT,
  WB_MOUSE_X1, /* Back */
  WB_MOUSE_X2, /* Forward */
  WB_MOUSE_BUTTON_COUNT
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
  WB_FILE_TYPE_UNKNOWN = 0,
  WB_FILE_TYPE_FILE,
  WB_FILE_TYPE_DIRECTORY,
  WB_FILE_TYPE_SYMLINK,
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
void Platform_RequestQuit(platform_window *window);
void Platform_SetFullscreen(platform_window *window, b32 fullscreen);
b32 Platform_IsFullscreen(platform_window *window);

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

/* ===== Clipboard File API =====
 * For copy/paste of files between applications.
 * Uses text/uri-list on Linux, CF_HDROP on Windows.
 */

/* Set files to OS clipboard. paths is array of absolute paths, count is number of paths.
 * is_cut: true for cut operation (files will be moved on paste), false for copy.
 * Returns true on success. */
b32 Platform_ClipboardSetFiles(const char **paths, i32 count, b32 is_cut);

/* Get files from OS clipboard. Returns number of paths retrieved (0 if none).
 * paths_out: array of buffers to store paths (each FS_MAX_PATH size).
 * max_paths: size of paths_out array.
 * is_cut_out: set to true if these were cut (move on paste), false if copy.
 * Returns: number of paths retrieved (0 if no files on clipboard). */
i32 Platform_ClipboardGetFiles(char **paths_out, i32 max_paths, b32 *is_cut_out);

/* ===== Process API ===== */

typedef struct platform_process platform_process;

platform_process *Platform_SpawnProcess(const char *command,
                                        const char *working_dir,
                                        b32 show_window);
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

/* ===== Cursor API ===== */

typedef enum {
  WB_CURSOR_DEFAULT,
  WB_CURSOR_POINTER,
  WB_CURSOR_TEXT,
  WB_CURSOR_GRAB,     /* Open hand */
  WB_CURSOR_GRABBING, /* Closed hand / dragging */
  WB_CURSOR_NO_DROP,  /* Circle with line through it */
  WB_CURSOR_COPY,     /* Arrow with plus */
} cursor_type;

void Platform_SetCursor(cursor_type cursor);

/* ===== Platform Initialization ===== */

b32 Platform_Init(void);
void Platform_Shutdown(void);

#endif /* PLATFORM_H */
