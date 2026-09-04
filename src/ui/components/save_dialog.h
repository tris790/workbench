/*
 * save_dialog.h - Workbench save picker
 *
 * A keyboard-first save dialog used by the application and by external
 * browser/desktop helpers.  The picker owns no OS window of its own; it is
 * rendered as a modal over the normal Workbench UI.
 */

#ifndef SAVE_DIALOG_H
#define SAVE_DIALOG_H

#include "../../core/fs.h"
#include "../../core/types.h"
#include "../ui.h"
#include "scroll_container.h"
#include "breadcrumb.h"
#include "quick_filter.h"
#include "../../core/text.h"

#define SAVE_DIALOG_SEARCH_SIZE 128
#define SAVE_DIALOG_ERROR_SIZE 256
#define SAVE_DIALOG_MAX_COMMON 8
#define SAVE_DIALOG_MAX_RESULTS 64
#define SAVE_DIALOG_MAX_CLIPBOARD 64

struct context_menu_state_s;

typedef enum {
  WB_SAVE_OPERATION_NONE = 0,
  WB_SAVE_OPERATION_RENAME,
  WB_SAVE_OPERATION_CREATE_FILE,
  WB_SAVE_OPERATION_CREATE_DIR,
  WB_SAVE_OPERATION_DELETE,
} save_dialog_operation;

typedef enum {
  WB_SAVE_FIELD_NAME = 0,
  WB_SAVE_FIELD_LIST,
} save_dialog_field;

typedef enum {
  WB_PICKER_SAVE_FILE = 0,
  WB_PICKER_OPEN_FILE,
  WB_PICKER_OPEN_FOLDER,
  WB_PICKER_OPEN_MULTI_FILE,
  WB_PICKER_SAVE_FILES,
} save_dialog_mode;

typedef struct save_dialog_state_s {
  memory_arena *arena;
  fs_state fs;
  scroll_container_state scroll;

  char source_path[FS_MAX_PATH];
  char name_buffer[FS_MAX_NAME];
  char result_path[FS_MAX_PATH];
  char error[SAVE_DIALOG_ERROR_SIZE];
  char overwrite_path[FS_MAX_PATH];
  char title[128];
  char accept_label[64];
  char result_paths[SAVE_DIALOG_MAX_RESULTS][FS_MAX_PATH];
  i32 result_count;
  save_dialog_mode mode;
  b32 pick_only;
  b32 external_result_only;
  b32 multiple;

  ui_text_state name_input;
  breadcrumb_state breadcrumb;
  quick_filter_state filter;
  char search_start_path[FS_MAX_PATH];
  b32 filter_was_active;

  i32 visible_indices[FS_MAX_ENTRIES];
  i32 visible_scores[FS_MAX_ENTRIES];
  i32 visible_count;
  i32 selected_index;
  save_dialog_field focused_field;

  char common_paths[SAVE_DIALOG_MAX_COMMON][FS_MAX_PATH];
  const char *common_labels[SAVE_DIALOG_MAX_COMMON];
  i32 common_count;

  rect list_bounds;
  u64 last_click_time;
  i32 last_click_index;

  b32 open;
  b32 finished;
  b32 accepted;
  b32 overwrite_pending;

  /* Context-menu file operations. */
  struct context_menu_state_s *context_menu;
  save_dialog_operation operation;
  char operation_buffer[256];
  ui_text_state operation_input;
  wrapped_text operation_text;
} save_dialog_state;

void SaveDialog_Init(save_dialog_state *state, memory_arena *arena);

/* Open a picker.  source_path may be NULL; in that case Save creates an empty
 * file, which makes the picker useful as a normal Workbench "Save As" UI. */
void SaveDialog_Open(save_dialog_state *state, const char *source_path,
                     const char *suggested_name);
void SaveDialog_OpenAt(save_dialog_state *state, const char *source_path,
                       const char *suggested_name, const char *start_path,
                       b32 pick_only);

/* Open a non-destructive picker for an external client such as a portal. */
void SaveDialog_OpenPicker(save_dialog_state *state, save_dialog_mode mode,
                           const char *title, const char *start_path,
                           const char *suggested_name, const char *accept_label,
                           b32 multiple);

/* Cancel the current picker, if it is open. */
void SaveDialog_Cancel(save_dialog_state *state, ui_context *ui);

void SaveDialog_Update(save_dialog_state *state, ui_context *ui);
void SaveDialog_Render(save_dialog_state *state, ui_context *ui, rect bounds);

/* File operations used by the shared context menu. */
void SaveDialog_Copy(save_dialog_state *state);
void SaveDialog_Cut(save_dialog_state *state);
void SaveDialog_Paste(save_dialog_state *state);
void SaveDialog_StartRename(save_dialog_state *state);
void SaveDialog_StartCreateFile(save_dialog_state *state);
void SaveDialog_StartCreateDir(save_dialog_state *state);
void SaveDialog_ConfirmDelete(save_dialog_state *state, ui_context *ui);

b32 SaveDialog_IsOpen(const save_dialog_state *state);
b32 SaveDialog_IsFinished(const save_dialog_state *state);
b32 SaveDialog_WasAccepted(const save_dialog_state *state);
const char *SaveDialog_GetResultPath(const save_dialog_state *state);
i32 SaveDialog_GetResultCount(const save_dialog_state *state);
const char *SaveDialog_GetResultPathAt(const save_dialog_state *state,
                                       i32 index);

#endif /* SAVE_DIALOG_H */
