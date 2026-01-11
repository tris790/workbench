/*
 * explorer.h - File Explorer Panel for Workbench
 *
 * Full-featured file explorer with navigation, icons, and file operations.
 * C99, handmade hero style.
 */

#ifndef EXPLORER_H
#define EXPLORER_H

#include "fs.h"
#include "fs_watcher.h"
#include "quick_filter.h"
#include "text.h"
#include "ui.h"

/* ===== Explorer State ===== */

#define EXPLORER_MAX_HISTORY 32
#define EXPLORER_DIALOG_WIDTH 420

/* Forward declaration */
struct context_menu_state_s;

typedef enum {
  EXPLORER_MODE_NORMAL,
  EXPLORER_MODE_RENAME,
  EXPLORER_MODE_CREATE_FILE,
  EXPLORER_MODE_CREATE_DIR,
  EXPLORER_MODE_CONFIRM_DELETE,
} explorer_mode;

typedef struct explorer_state_s {
  /* File system state */
  fs_state fs;

  /* Navigation History */
  char history[EXPLORER_MAX_HISTORY][FS_MAX_PATH];
  i32 history_index; /* Current position in history */
  i32 history_count; /* Total valid items in history */

  /* UI state */
  ui_scroll_state scroll;
  i32 item_height;
  b32 show_hidden;
  b32 show_size_column;
  b32 show_date_column;

  /* Mode and dialogs */
  explorer_mode mode;
  char input_buffer[256];
  ui_text_state input_state;

  /* Double-click detection */
  u64 last_click_time;
  i32 last_click_index;

  /* For smooth selection animation */
  smooth_value selection_anim;

  /* Clipboard for copy/paste */
  char clipboard_path[FS_MAX_PATH];
  b32 clipboard_is_cut;

  /* Cached bounds for mouse input */
  rect list_bounds;

  /* Flag to trigger auto-scroll to selection (set on keyboard nav) */
  b32 scroll_to_selection;

  /* Wrapped text for dialogs (arena-allocated) */
  wrapped_text dialog_text;

  /* Quick filter state */
  quick_filter_state filter;

  /* Search Traversal State */
  char search_start_path[FS_MAX_PATH];
  b32 filter_was_active;
  char last_filter_buffer[QUICK_FILTER_MAX_INPUT];

  /* Context menu (managed by layout/main.c) */
  struct context_menu_state_s *context_menu;

  /* File system watcher for external changes */
  fs_watcher watcher;
} explorer_state;

/* ===== Explorer API ===== */

/* Initialize explorer state */
void Explorer_Init(explorer_state *state, memory_arena *arena);

/* Shutdown and cleanup explorer resources */
void Explorer_Shutdown(explorer_state *state);

/* Poll file watcher for external changes (call for ALL panels each frame) */
void Explorer_PollWatcher(explorer_state *state);

/* Update explorer (call each frame, handles input) */
void Explorer_Update(explorer_state *state, ui_context *ui);

/* Render explorer panel in given bounds */
void Explorer_Render(explorer_state *state, ui_context *ui, rect bounds,
                     b32 has_focus);

/* Navigate to specific path */
b32 Explorer_NavigateTo(explorer_state *state, const char *path);

/* Refresh current directory */
void Explorer_Refresh(explorer_state *state);

/* Navigation History */
void Explorer_GoBack(explorer_state *state);
void Explorer_GoForward(explorer_state *state);

/* Get currently selected entry (NULL if none) */
fs_entry *Explorer_GetSelected(explorer_state *state);

/* Toggle hidden files visibility */
void Explorer_ToggleHidden(explorer_state *state);

/* Start file operations */
void Explorer_StartRename(explorer_state *state);
void Explorer_StartCreateFile(explorer_state *state);
void Explorer_StartCreateDir(explorer_state *state);
void Explorer_ConfirmDelete(explorer_state *state, ui_context *ui);
void Explorer_Copy(explorer_state *state);
void Explorer_Cut(explorer_state *state);
void Explorer_Paste(explorer_state *state);

/* Cancel current operation */
void Explorer_Cancel(explorer_state *state);

#endif /* EXPLORER_H */
