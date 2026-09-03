/*
 * save_dialog.c - Workbench save picker implementation
 */

#include "save_dialog.h"
#include "../../core/fuzzy_match.h"
#include "../../core/input.h"
#include "../../core/theme.h"
#include "breadcrumb.h"
#include "file_item.h"
#include "dialog.h"
#include "quick_filter.h"

#include <stdio.h>
#include <string.h>

#define SAVE_DIALOG_ROW_HEIGHT 28
#define SAVE_DIALOG_SIDEBAR_WIDTH 156
#define SAVE_DIALOG_MAX_COMMON_PATHS SAVE_DIALOG_MAX_COMMON
#define SAVE_DIALOG_BREADCRUMB_HEIGHT 32

static void SaveDialog_CopyString(char *destination, usize destination_size,
                                  const char *source) {
  usize length;
  if (!destination || destination_size == 0)
    return;
  if (!source) {
    destination[0] = '\0';
    return;
  }

  length = strlen(source);
  if (length >= destination_size)
    length = destination_size - 1;
  memcpy(destination, source, length);
  destination[length] = '\0';
}

static void SaveDialog_SetField(save_dialog_state *state, ui_context *ui,
                                save_dialog_field field) {
  state->focused_field = field;
  state->name_input.has_focus = false;

  if (field == WB_SAVE_FIELD_NAME) {
    ui->focused = UI_GenID("Save filename");
    state->name_input.has_focus = true;
  } else {
    /* The list is navigated explicitly; it is intentionally not part of the
     * generic focus-order list so Up/Down remain directory navigation keys. */
    ui->focused = UI_ID_NONE;
  }
}

static void SaveDialog_RebuildVisible(save_dialog_state *state) {
  const char *query = QuickFilter_GetQuery(&state->filter);
  state->visible_count = 0;

  for (i32 i = 0; i < (i32)state->fs.entry_count; i++) {
    fs_entry *entry = &state->fs.entries[i];
    const char *match_query = query;
    const char *last_separator = FS_FindLastSeparator(query);
    if (last_separator)
      match_query = last_separator + 1;

    fuzzy_match_result match = FuzzyMatchScore(match_query, entry->name);
    if (strcmp(entry->name, "..") != 0 && match_query[0] != '\0' &&
        !match.matches)
      continue;

    i32 insert = state->visible_count;
    while (insert > 0) {
      i32 previous = state->visible_scores[insert - 1];
      b32 previous_is_dir =
          state->fs.entries[state->visible_indices[insert - 1]].is_directory;
      if (previous > match.score ||
          (previous == match.score && previous_is_dir >= entry->is_directory)) {
        break;
      }
      state->visible_indices[insert] = state->visible_indices[insert - 1];
      state->visible_scores[insert] = previous;
      insert--;
    }

    if (insert < FS_MAX_ENTRIES) {
      state->visible_indices[insert] = i;
      state->visible_scores[insert] = match.score;
      state->visible_count++;
    }
  }

  /* Keep the selection valid after filtering. */
  b32 selected_visible = false;
  for (i32 i = 0; i < state->visible_count; i++) {
    if (state->visible_indices[i] == state->selected_index) {
      selected_visible = true;
      break;
    }
  }
  if (!selected_visible && state->visible_count > 0) {
    state->selected_index = state->visible_indices[0];
    FS_SetSelection(&state->fs, state->selected_index);
  }
}

static void SaveDialog_AddCommon(save_dialog_state *state, const char *label,
                                 const char *path) {
  if (state->common_count >= SAVE_DIALOG_MAX_COMMON_PATHS ||
      !path || !Platform_IsDirectory(path))
    return;

  for (i32 i = 0; i < state->common_count; i++) {
    if (FS_PathsEqual(state->common_paths[i], path))
      return;
  }

  SaveDialog_CopyString(state->common_paths[state->common_count], FS_MAX_PATH,
                        path);
  state->common_labels[state->common_count] = label;
  state->common_count++;
}

static void SaveDialog_BuildCommonLocations(save_dialog_state *state) {
  char home[FS_MAX_PATH];
  char downloads[FS_MAX_PATH];
  char candidate[FS_MAX_PATH];

  state->common_count = 0;
  if (!Platform_GetHomePath(home, sizeof(home)))
    return;

  SaveDialog_AddCommon(state, "Home", home);

  if (Platform_GetDownloadsPath(downloads, sizeof(downloads)))
    SaveDialog_AddCommon(state, "Downloads", downloads);

  FS_JoinPath(candidate, sizeof(candidate), home, "Desktop");
  SaveDialog_AddCommon(state, "Desktop", candidate);
  FS_JoinPath(candidate, sizeof(candidate), home, "Documents");
  SaveDialog_AddCommon(state, "Documents", candidate);
  FS_JoinPath(candidate, sizeof(candidate), home, "Pictures");
  SaveDialog_AddCommon(state, "Pictures", candidate);
  FS_JoinPath(candidate, sizeof(candidate), home, "Music");
  SaveDialog_AddCommon(state, "Music", candidate);
  FS_JoinPath(candidate, sizeof(candidate), home, "Videos");
  SaveDialog_AddCommon(state, "Videos", candidate);
}

static void SaveDialog_SetError(save_dialog_state *state, const char *message) {
  SaveDialog_CopyString(state->error, sizeof(state->error), message);
}

static void SaveDialog_ResetDirectoryUi(save_dialog_state *state) {
  state->scroll.offset = (v2f){0, 0};
  state->scroll.target_offset = (v2f){0, 0};
  state->scroll.scroll_v.current = 0;
  state->scroll.scroll_v.target = 0;
  state->overwrite_pending = false;
  state->overwrite_path[0] = '\0';
  state->last_click_index = -1;
  state->error[0] = '\0';
}

static b32 SaveDialog_PathIsWithin(const char *path, const char *root,
                                   char *relative, usize relative_size) {
  char normalized_path[FS_MAX_PATH];
  char normalized_root[FS_MAX_PATH];
  usize root_length;
  b32 root_is_absolute = false;

  if (!path || !root || !relative || relative_size == 0)
    return false;
  SaveDialog_CopyString(normalized_path, sizeof(normalized_path), path);
  SaveDialog_CopyString(normalized_root, sizeof(normalized_root), root);
  FS_NormalizePath(normalized_path);
  FS_NormalizePath(normalized_root);
  if (FS_PathsEqual(normalized_path, normalized_root)) {
    relative[0] = '\0';
    return true;
  }

  root_length = strlen(normalized_root);
  root_is_absolute = (root_length == 1 && FS_IsPathSeparator(normalized_root[0])) ||
                     (root_length == 3 && normalized_root[1] == ':' &&
                      FS_IsPathSeparator(normalized_root[2]));
  if (strncmp(normalized_path, normalized_root, root_length) != 0 ||
      (!root_is_absolute && !FS_IsPathSeparator(normalized_path[root_length])))
    return false;

  SaveDialog_CopyString(relative, relative_size,
                        normalized_path + root_length + (root_is_absolute ? 0 : 1));
  return true;
}

static void SaveDialog_SyncFilterToPath(save_dialog_state *state) {
  char relative[FS_MAX_PATH];
  if (!QuickFilter_IsActive(&state->filter))
    return;

  if (!SaveDialog_PathIsWithin(state->fs.current_path, state->search_start_path,
                               relative, sizeof(relative))) {
    QuickFilter_Clear(&state->filter);
  } else if (relative[0] == '\0') {
    QuickFilter_ClearBuffer(&state->filter);
  } else {
    char filter_text[FS_MAX_PATH];
    usize relative_length = strlen(relative);
    if (relative_length + 1 >= sizeof(filter_text))
      relative_length = sizeof(filter_text) - 2;
    memcpy(filter_text, relative, relative_length);
    filter_text[relative_length++] = '/';
    filter_text[relative_length] = '\0';
    QuickFilter_SetBuffer(&state->filter, filter_text);
  }
}

static void SaveDialog_Navigate(save_dialog_state *state, const char *path,
                                b32 keep_filter) {
  if (!path || !Platform_IsDirectory(path)) {
    SaveDialog_SetError(state, "That location is not available.");
    return;
  }

  if (!FS_LoadDirectory(&state->fs, path)) {
    SaveDialog_SetError(state, "Workbench could not read that folder.");
    return;
  }

  state->selected_index = state->fs.selected_index;
  SaveDialog_ResetDirectoryUi(state);
  if (keep_filter) {
    SaveDialog_SyncFilterToPath(state);
  } else {
    QuickFilter_Clear(&state->filter);
  }
  SaveDialog_RebuildVisible(state);
}

static void SaveDialog_NavigateUp(save_dialog_state *state) {
  if (FS_NavigateUp(&state->fs)) {
    state->selected_index = state->fs.selected_index;
    SaveDialog_ResetDirectoryUi(state);
    SaveDialog_SyncFilterToPath(state);
    SaveDialog_RebuildVisible(state);
  }
}

static void SaveDialog_Finish(save_dialog_state *state, ui_context *ui,
                              b32 accepted) {
  state->open = false;
  state->finished = true;
  state->accepted = accepted;
  Input_PopFocus();
  if (ui) {
    ui->next_modal = UI_ID_NONE;
    ui->current_modal = UI_ID_NONE;
  }
}

static void SaveDialog_ClearResults(save_dialog_state *state) {
  state->result_count = 0;
  state->result_paths[0][0] = '\0';
}

static void SaveDialog_AddResult(save_dialog_state *state, const char *path) {
  if (!path || !path[0] || state->result_count >= SAVE_DIALOG_MAX_RESULTS)
    return;

  SaveDialog_CopyString(state->result_paths[state->result_count], FS_MAX_PATH,
                        path);
  if (state->result_count == 0)
    SaveDialog_CopyString(state->result_path, FS_MAX_PATH, path);
  state->result_count++;
}

static b32 SaveDialog_IsOpenMode(const save_dialog_state *state) {
  return state->mode == WB_PICKER_OPEN_FILE ||
         state->mode == WB_PICKER_OPEN_MULTI_FILE ||
         state->mode == WB_PICKER_OPEN_FOLDER;
}

static b32 SaveDialog_HasNameField(const save_dialog_state *state) {
  return state->mode == WB_PICKER_SAVE_FILE && !state->pick_only;
}

static b32 SaveDialog_CommitOpen(save_dialog_state *state, ui_context *ui) {
  if (state->mode == WB_PICKER_OPEN_FOLDER) {
    fs_entry *entry = FS_GetSelectedEntry(&state->fs);
    if (!entry || !entry->is_directory || strcmp(entry->name, "..") == 0) {
      SaveDialog_SetError(state, "Choose a folder first.");
      return false;
    }

    SaveDialog_ClearResults(state);
    SaveDialog_AddResult(state, entry->path);
  } else {
    SaveDialog_ClearResults(state);

    for (i32 index = FS_GetFirstSelected(&state->fs); index >= 0;
         index = FS_GetNextSelected(&state->fs, index)) {
      fs_entry *entry = FS_GetEntry(&state->fs, index);
      if (entry && !entry->is_directory)
        SaveDialog_AddResult(state, entry->path);
    }

    if (state->result_count == 0) {
      fs_entry *entry = FS_GetSelectedEntry(&state->fs);
      if (entry && !entry->is_directory)
        SaveDialog_AddResult(state, entry->path);
    }

    if (state->result_count == 0) {
      SaveDialog_SetError(state, "Choose a file first.");
      return false;
    }
  }

  SaveDialog_Finish(state, ui, true);
  return true;
}

static b32 SaveDialog_Commit(save_dialog_state *state, ui_context *ui) {
  if (SaveDialog_IsOpenMode(state))
    return SaveDialog_CommitOpen(state, ui);

  if (state->mode == WB_PICKER_SAVE_FILES) {
    SaveDialog_ClearResults(state);
    SaveDialog_CopyString(state->result_path, FS_MAX_PATH,
                          state->fs.current_path);
    SaveDialog_AddResult(state, state->fs.current_path);
    SaveDialog_Finish(state, ui, true);
    return true;
  }

  if (state->name_buffer[0] == '\0') {
    SaveDialog_SetError(state, "Choose a file name first.");
    return false;
  }

  /* A save name is a basename.  Navigation is deliberately separate so a
   * browser cannot smuggle an unintended path through a suggested filename. */
  if (strpbrk(state->name_buffer, "/\\:*?\"<>|")) {
    SaveDialog_SetError(state, "Choose a portable file name (no / \\ : * ? \" < > |).");
    return false;
  }
  if (strcmp(state->name_buffer, ".") == 0 ||
      strcmp(state->name_buffer, "..") == 0) {
    SaveDialog_SetError(state, "That name is reserved for directory navigation.");
    return false;
  }

  char destination[FS_MAX_PATH];
  FS_JoinPath(destination, sizeof(destination), state->fs.current_path,
              state->name_buffer);

  if (Platform_IsDirectory(destination)) {
    SaveDialog_SetError(state, "A folder already has that name.");
    return false;
  }

  if (Platform_FileExists(destination)) {
    if (!state->overwrite_pending ||
        !FS_PathsEqual(state->overwrite_path, destination)) {
      state->overwrite_pending = true;
      SaveDialog_CopyString(state->overwrite_path, FS_MAX_PATH, destination);
      SaveDialog_SetError(
          state, "That file already exists. Press Save again to replace it.");
      return false;
    }
  } else {
    state->overwrite_pending = false;
    state->overwrite_path[0] = '\0';
  }

  if (state->pick_only || state->external_result_only) {
    SaveDialog_CopyString(state->result_path, FS_MAX_PATH, destination);
    SaveDialog_ClearResults(state);
    SaveDialog_AddResult(state, destination);
    state->overwrite_pending = false;
    state->overwrite_path[0] = '\0';
    SaveDialog_Finish(state, ui, true);
    return true;
  }

  b32 success;
  if (state->source_path[0] != '\0') {
    if (!Platform_FileExists(state->source_path)) {
      SaveDialog_SetError(state, "The source file is no longer available.");
      return false;
    }
    /* A browser may already have placed the temporary file in the chosen
     * folder.  Treat selecting that exact path as success instead of copying
     * a file onto itself (which would truncate it on some platforms). */
    if (FS_PathsEqual(state->source_path, destination))
      success = true;
    else
      success = FS_CopyRecursive(state->source_path, destination, state->arena);
  } else {
    success = FS_CreateFile(destination);
  }

  if (!success) {
    SaveDialog_SetError(state, "Workbench could not write that file.");
    return false;
  }

  SaveDialog_CopyString(state->result_path, FS_MAX_PATH, destination);
  state->overwrite_pending = false;
  state->overwrite_path[0] = '\0';
  SaveDialog_Finish(state, ui, true);
  return true;
}

static void SaveDialog_MoveSelection(save_dialog_state *state, i32 delta) {
  if (state->visible_count <= 0)
    return;

  i32 position = 0;
  for (i32 i = 0; i < state->visible_count; i++) {
    if (state->visible_indices[i] == state->selected_index) {
      position = i;
      break;
    }
  }

  position = Clamp(position + delta, 0, state->visible_count - 1);
  state->selected_index = state->visible_indices[position];
  FS_SetSelection(&state->fs, state->selected_index);
  ScrollContainer_ScrollToY(&state->scroll,
                            (f32)(position * SAVE_DIALOG_ROW_HEIGHT),
                            SAVE_DIALOG_ROW_HEIGHT);
}

static void SaveDialog_UpdateFilterNavigation(save_dialog_state *state) {
  const char *query = QuickFilter_GetQuery(&state->filter);
  const char *last_separator = FS_FindLastSeparator(query);
  char path_part[FS_MAX_PATH] = {0};
  char target_path[FS_MAX_PATH] = {0};

  if (!query[0] || !last_separator)
    return;

  usize length = (usize)(last_separator - query) + 1;
  if (length >= sizeof(path_part))
    return;
  memcpy(path_part, query, length);
  path_part[length] = '\0';

  if (path_part[0] == '/' ||
      (path_part[0] && path_part[1] == ':' && FS_IsPathSeparator(path_part[2]))) {
    SaveDialog_CopyString(target_path, sizeof(target_path), path_part);
  } else if (path_part[0] == '~') {
    if (!FS_ResolvePath(path_part, target_path, sizeof(target_path)))
      return;
  } else {
    FS_JoinPath(target_path, sizeof(target_path), state->search_start_path,
                path_part);
  }

  usize target_length = strlen(target_path);
  while (target_length > 1 &&
         FS_IsPathSeparator(target_path[target_length - 1])) {
    target_path[--target_length] = '\0';
  }

  if (!Platform_IsDirectory(target_path) ||
      FS_PathsEqual(state->fs.current_path, target_path))
    return;

  /* Keep the filename portion of the query after entering its directory. */
  char query_copy[SAVE_DIALOG_SEARCH_SIZE];
  SaveDialog_CopyString(query_copy, sizeof(query_copy), query);
  SaveDialog_Navigate(state, target_path, false);
  QuickFilter_SetBuffer(&state->filter, query_copy);
}

void SaveDialog_Init(save_dialog_state *state, memory_arena *arena) {
  memset(state, 0, sizeof(*state));
  state->arena = arena;
  FS_Init(&state->fs, arena);
  ScrollContainer_Init(&state->scroll);
  /* The list owns initial focus.  Typing therefore starts find mode; the
   * filename editor is an explicit, secondary target. */
  state->focused_field = WB_SAVE_FIELD_LIST;
  state->mode = WB_PICKER_SAVE_FILE;
  SaveDialog_CopyString(state->title, sizeof(state->title), "Save file");
  SaveDialog_CopyString(state->accept_label, sizeof(state->accept_label),
                        "Save");
  state->last_click_index = -1;
  state->name_input.selection_start = -1;
  QuickFilter_Init(&state->filter);
  state->search_start_path[0] = '\0';
  state->filter_was_active = false;
  SaveDialog_BuildCommonLocations(state);
}

void SaveDialog_Open(save_dialog_state *state, const char *source_path,
                     const char *suggested_name) {
  SaveDialog_OpenAt(state, source_path, suggested_name, NULL, false);
}

void SaveDialog_OpenAt(save_dialog_state *state, const char *source_path,
                       const char *suggested_name, const char *start_path,
                       b32 pick_only) {
  char home[FS_MAX_PATH];
  char downloads[FS_MAX_PATH];
  const char *start = NULL;
  b32 was_open = state->open;

  state->open = true;
  state->finished = false;
  state->accepted = false;
  state->pick_only = pick_only;
  state->external_result_only = false;
  state->mode = WB_PICKER_SAVE_FILE;
  state->multiple = false;
  SaveDialog_CopyString(state->title, sizeof(state->title), "Save file");
  SaveDialog_CopyString(state->accept_label, sizeof(state->accept_label),
                        "Save");
  state->overwrite_pending = false;
  state->overwrite_path[0] = '\0';
  state->last_click_index = -1;
  state->result_path[0] = '\0';
  SaveDialog_ClearResults(state);
  state->error[0] = '\0';
  QuickFilter_Clear(&state->filter);
  state->search_start_path[0] = '\0';
  state->filter_was_active = false;
  state->name_input = (ui_text_state){0};
  state->name_input.selection_start = -1;
  state->focused_field = WB_SAVE_FIELD_LIST;

  if (source_path) {
    SaveDialog_CopyString(state->source_path, FS_MAX_PATH, source_path);
  } else {
    state->source_path[0] = '\0';
  }

  if (suggested_name && suggested_name[0]) {
    SaveDialog_CopyString(state->name_buffer, FS_MAX_NAME, suggested_name);
  } else {
    SaveDialog_CopyString(state->name_buffer, FS_MAX_NAME, "untitled");
  }
  state->name_input.cursor_pos = (i32)strlen(state->name_buffer);
  state->name_input.selection_start = 0;
  state->name_input.selection_end = state->name_input.cursor_pos;
  state->name_input.has_focus = false;

  if (start_path && Platform_IsDirectory(start_path)) {
    start = start_path;
  } else if (Platform_GetDownloadsPath(downloads, sizeof(downloads)) &&
             Platform_IsDirectory(downloads)) {
    start = downloads;
  } else if (Platform_GetHomePath(home, sizeof(home))) {
    start = home;
  }

  if (start) {
    SaveDialog_Navigate(state, start, false);
  } else {
    SaveDialog_SetError(state, "Workbench could not find a writable location.");
    state->visible_count = 0;
  }

  if (!was_open)
    Input_PushFocus(WB_INPUT_TARGET_DIALOG);
  else
    Input_SetFocus(WB_INPUT_TARGET_DIALOG);
}

void SaveDialog_OpenPicker(save_dialog_state *state, save_dialog_mode mode,
                           const char *title, const char *start_path,
                           const char *suggested_name, const char *accept_label,
                           b32 multiple) {
  char home[FS_MAX_PATH];
  char downloads[FS_MAX_PATH];
  const char *start = NULL;
  b32 was_open = state->open;

  state->open = true;
  state->finished = false;
  state->accepted = false;
  state->pick_only = false;
  state->external_result_only = true;
  state->source_path[0] = '\0';
  state->mode = mode;
  state->multiple = multiple && mode == WB_PICKER_OPEN_MULTI_FILE;
  state->overwrite_pending = false;
  state->overwrite_path[0] = '\0';
  state->last_click_index = -1;
  state->result_path[0] = '\0';
  SaveDialog_ClearResults(state);
  state->error[0] = '\0';
  QuickFilter_Clear(&state->filter);
  state->search_start_path[0] = '\0';
  state->filter_was_active = false;
  state->name_input = (ui_text_state){0};
  state->name_input.selection_start = -1;
  /* All picker modes start in the file list.  Filename editing is reached by
   * clicking the field (or pressing Tab), preserving find-on-type behavior. */
  state->focused_field = WB_SAVE_FIELD_LIST;

  SaveDialog_CopyString(state->title, sizeof(state->title),
                        title && title[0] ? title :
                        (SaveDialog_IsOpenMode(state) ? "Open file" :
                                                        "Save file"));
  SaveDialog_CopyString(
      state->accept_label, sizeof(state->accept_label),
      accept_label && accept_label[0]
          ? accept_label
          : (SaveDialog_IsOpenMode(state) ? "Open" : "Save"));

  if (suggested_name && suggested_name[0])
    SaveDialog_CopyString(state->name_buffer, FS_MAX_NAME, suggested_name);
  else
    SaveDialog_CopyString(state->name_buffer, FS_MAX_NAME, "untitled");

  state->name_input.cursor_pos = (i32)strlen(state->name_buffer);
  state->name_input.selection_start = 0;
  state->name_input.selection_end = state->name_input.cursor_pos;
  state->name_input.has_focus = false;

  if (start_path && Platform_IsDirectory(start_path)) {
    start = start_path;
  } else if (Platform_GetDownloadsPath(downloads, sizeof(downloads)) &&
             Platform_IsDirectory(downloads)) {
    start = downloads;
  } else if (Platform_GetHomePath(home, sizeof(home))) {
    start = home;
  }

  if (start) {
    SaveDialog_Navigate(state, start, false);
  } else {
    SaveDialog_SetError(state, "Workbench could not find a usable location.");
    state->visible_count = 0;
  }

  if (!was_open)
    Input_PushFocus(WB_INPUT_TARGET_DIALOG);
  else
    Input_SetFocus(WB_INPUT_TARGET_DIALOG);
}

void SaveDialog_Cancel(save_dialog_state *state, ui_context *ui) {
  if (state && state->open)
    SaveDialog_Finish(state, ui, false);
}

void SaveDialog_Update(save_dialog_state *state, ui_context *ui) {
  if (!state->open)
    return;

  if (state->list_bounds.w > 0 && state->list_bounds.h > 0) {
    ScrollContainer_SetContentSize(
        &state->scroll, (f32)(state->visible_count * SAVE_DIALOG_ROW_HEIGHT));
    ScrollContainer_Update(&state->scroll, ui, state->list_bounds);
  }

  ui_input *input = &ui->input;

  if (input->key_pressed[WB_KEY_TAB]) {
    if (SaveDialog_HasNameField(state)) {
      save_dialog_field next =
          (save_dialog_field)((state->focused_field + 1) % 2);
      SaveDialog_SetField(state, ui, next);
    } else {
      SaveDialog_SetField(state, ui, WB_SAVE_FIELD_LIST);
    }
    input->key_pressed[WB_KEY_TAB] = false;
  }

  if (state->focused_field == WB_SAVE_FIELD_LIST) {
    b32 filter_was_active = QuickFilter_IsActive(&state->filter);
    b32 filter_escape_pressed = filter_was_active &&
                                input->key_pressed[WB_KEY_ESCAPE];
    QuickFilter_Update(&state->filter, ui);
    b32 filter_is_active = QuickFilter_IsActive(&state->filter);

    /* Escape closes find first.  A second Escape (with find already closed)
     * cancels the picker itself. */
    if (filter_escape_pressed) {
      input->key_pressed[WB_KEY_ESCAPE] = false;
      state->filter_was_active = false;
      return;
    }

    if (!filter_was_active && filter_is_active) {
      SaveDialog_CopyString(state->search_start_path, FS_MAX_PATH,
                            state->fs.current_path);
    }
    if (filter_is_active)
      SaveDialog_UpdateFilterNavigation(state);
    state->filter_was_active = filter_is_active;

    if (input->key_pressed[WB_KEY_UP] || Input_KeyRepeat(WB_KEY_UP)) {
      SaveDialog_MoveSelection(state, -1);
      input->key_pressed[WB_KEY_UP] = false;
    }
    if (input->key_pressed[WB_KEY_DOWN] || Input_KeyRepeat(WB_KEY_DOWN)) {
      SaveDialog_MoveSelection(state, 1);
      input->key_pressed[WB_KEY_DOWN] = false;
    }
    if (input->key_pressed[WB_KEY_RETURN] && !filter_is_active) {
      fs_entry *entry = FS_GetEntry(&state->fs, state->selected_index);
      if (entry && entry->is_directory) {
        if (strcmp(entry->name, "..") == 0)
          SaveDialog_NavigateUp(state);
        else
          SaveDialog_Navigate(state, entry->path, false);
      } else {
        SaveDialog_Commit(state, ui);
      }
      input->key_pressed[WB_KEY_RETURN] = false;
    } else if (input->key_pressed[WB_KEY_RETURN] && filter_is_active) {
      /* Return is a file action only after find has been dismissed. */
      input->key_pressed[WB_KEY_RETURN] = false;
    }
  } else if (input->key_pressed[WB_KEY_RETURN] &&
             !QuickFilter_IsActive(&state->filter)) {
    SaveDialog_Commit(state, ui);
    input->key_pressed[WB_KEY_RETURN] = false;
  } else if (input->key_pressed[WB_KEY_RETURN]) {
    input->key_pressed[WB_KEY_RETURN] = false;
  }

  if (input->key_pressed[WB_KEY_ESCAPE]) {
    SaveDialog_Finish(state, ui, false);
    input->key_pressed[WB_KEY_ESCAPE] = false;
  }

  /* Up/Down are consumed above while the list owns focus.  The filename field
   * keeps those keys so the shared text editor can move its cursor. */
}

static void SaveDialog_RenderRow(save_dialog_state *state, ui_context *ui,
                                 i32 visible_index, rect row) {
  i32 actual_index = state->visible_indices[visible_index];
  fs_entry *entry = FS_GetEntry(&state->fs, actual_index);
  if (!entry)
    return;

  UI_PushID("SaveRow");
  UI_PushIDInt(actual_index);
  ui_id id = UI_GenID("row");
  b32 clicked = UI_UpdateInteraction(id, row);
  UI_PopID();
  UI_PopID();

  b32 hovered = UI_PointInRect(ui->input.mouse_pos, row);
  if (clicked) {
    SaveDialog_SetField(state, ui, WB_SAVE_FIELD_LIST);
    state->selected_index = actual_index;

    if (state->multiple) {
      if (ui->input.modifiers & MOD_SHIFT) {
        i32 active_position = visible_index;
        i32 anchor_position = 0;
        i32 anchor = state->fs.selection_anchor;
        for (i32 i = 0; i < state->visible_count; i++) {
          if (state->visible_indices[i] == anchor)
            anchor_position = i;
        }
        FS_SelectOrderedRange(&state->fs, state->visible_indices,
                              state->visible_count, anchor_position,
                              active_position, anchor, actual_index);
      } else if (ui->input.modifiers & MOD_CTRL) {
        FS_SelectToggle(&state->fs, actual_index);
      } else {
        FS_SelectSingle(&state->fs, actual_index);
      }
    } else {
      FS_SetSelection(&state->fs, actual_index);
      if (!entry->is_directory && state->mode == WB_PICKER_SAVE_FILE) {
        /* Selecting an existing file supplies its name to Save As.  The list
         * remains focused, so subsequent typing still starts find mode. */
        SaveDialog_CopyString(state->name_buffer, FS_MAX_NAME, entry->name);
        state->name_input.cursor_pos = (i32)strlen(state->name_buffer);
        state->name_input.selection_start = -1;
        state->name_input.selection_end = state->name_input.cursor_pos;
      }
    }

    u64 now = Platform_GetTimeMs();
    if (state->last_click_index == actual_index &&
        now - state->last_click_time < 450) {
      if (entry->is_directory) {
        if (strcmp(entry->name, "..") == 0)
          SaveDialog_NavigateUp(state);
        else
          SaveDialog_Navigate(state, entry->path, false);
        state->selected_index = state->fs.selected_index;
        state->scroll.offset = (v2f){0, 0};
        state->scroll.target_offset = (v2f){0, 0};
        SaveDialog_RebuildVisible(state);
      } else if (SaveDialog_IsOpenMode(state)) {
        SaveDialog_Commit(state, ui);
      }
    }
    state->last_click_index = actual_index;
    state->last_click_time = now;
  }

  file_item_config config = {.icon_size = 18, .icon_padding = 8, .show_size = true};
  FileItem_Render(ui, entry, row, FS_IsSelected(&state->fs, actual_index), hovered,
                  &config);
}

void SaveDialog_Render(save_dialog_state *state, ui_context *ui, rect bounds) {
  if (!state->open)
    return;

  const theme *th = ui->theme;
  dialog_shell_config shell_config = {
      .modal_name = "SaveDialog",
      .title = state->title[0] ? state->title : "Save file",
      /* Keep the picker responsive: the shell leaves only its 24px edge
       * padding instead of imposing a desktop-sized fixed rectangle. */
      .preferred_width = Max(1, bounds.w - 48),
      .preferred_height = Max(1, bounds.h - 48),
      .min_width = 680,
      .min_height = 460,
      .margin_x = 24,
      .margin_y = 24,
  };
  dialog_shell_layout shell = Dialog_BeginShell(ui, bounds, &shell_config);
  rect content = shell.content;
  i32 top_h = 36;
  i32 footer_h = 76;
  b32 has_name_field = SaveDialog_HasNameField(state);
  i32 name_h = has_name_field ? 62 : 0;
  i32 sidebar_w = SAVE_DIALOG_SIDEBAR_WIDTH;
  i32 gap = th->spacing_lg;

  /* Use the same clickable breadcrumb as the explorer. */
  rect top = {content.x, content.y, content.w, top_h};
  breadcrumb_result breadcrumb = Breadcrumb_Render(
      ui, top, state->fs.current_path, &state->breadcrumb);
  if (breadcrumb.clicked_segment >= 0) {
    char target_path[FS_MAX_PATH];
    if (Breadcrumb_GetPathForSegment(state->fs.current_path,
                                     breadcrumb.clicked_segment, target_path,
                                     sizeof(target_path)))
      SaveDialog_Navigate(state, target_path,
                           QuickFilter_IsActive(&state->filter));
  }
  if (breadcrumb.text_changed || breadcrumb.editing_finished) {
    char resolved_path[FS_MAX_PATH];
    if (FS_ResolvePath(state->breadcrumb.edit_buffer, resolved_path,
                       sizeof(resolved_path))) {
      if (!Platform_IsDirectory(resolved_path))
        FS_FindDeepestValidDirectory(resolved_path, resolved_path,
                                     sizeof(resolved_path));
      SaveDialog_Navigate(state, resolved_path,
                          QuickFilter_IsActive(&state->filter));
    }
  }

  i32 body_y = top.y + top_h + gap;
  i32 body_h = content.h - top_h - gap - footer_h - name_h -
               (has_name_field ? gap : 0);
  if (body_h < 80)
    body_h = 80;

  /* Common locations sidebar. */
  rect sidebar = {content.x, body_y, sidebar_w, body_h};
  Render_DrawRectRounded(ui->renderer, sidebar, th->radius_sm,
                         Color_WithAlpha(th->panel_alt, 160));
  Render_DrawText(ui->renderer, (v2i){sidebar.x + 12, sidebar.y + 10},
                  "Places", ui->font, th->text_muted);
  UI_PushID("SavePlaces");
  i32 place_y = sidebar.y + 36;
  for (i32 i = 0; i < state->common_count; i++) {
    rect place = {sidebar.x + 6, place_y, sidebar.w - 12, 30};
    UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, place);
    UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, place.w);
    UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, place.h);
    if (UI_Button(state->common_labels[i]))
      SaveDialog_Navigate(state, state->common_paths[i], false);
    UI_PopStyleN(2);
    UI_EndLayout();
    place_y += 34;
  }
  UI_PopID();

  /* Main list/search column. */
  i32 main_x = sidebar.x + sidebar.w + gap;
  i32 main_w = content.w - sidebar.w - gap;
  rect main = {main_x, body_y, main_w, body_h};
  /* The shared explorer filter is an overlay and appears after the first
   * printable character. */
  SaveDialog_RebuildVisible(state);

  rect list = {main.x, main.y, main.w, main.h};
  state->list_bounds = list;
  ScrollContainer_SetContentSize(
      &state->scroll, (f32)(state->visible_count * SAVE_DIALOG_ROW_HEIGHT));
  Render_SetClipRect(ui->renderer, list);
  i32 start = (i32)(state->scroll.offset.y / SAVE_DIALOG_ROW_HEIGHT);
  i32 end = start + list.h / SAVE_DIALOG_ROW_HEIGHT + 2;
  if (start < 0)
    start = 0;
  if (end > state->visible_count)
    end = state->visible_count;
  for (i32 i = start; i < end; i++) {
    rect row = {list.x, list.y + i * SAVE_DIALOG_ROW_HEIGHT -
                           (i32)state->scroll.offset.y,
                list.w - SCROLL_SCROLLBAR_GUTTER, SAVE_DIALOG_ROW_HEIGHT};
    SaveDialog_RenderRow(state, ui, i, row);
  }
  Render_ResetClipRect(ui->renderer);
  ScrollContainer_RenderScrollbar(&state->scroll, ui);
  QuickFilter_Render(&state->filter, ui, list);

  if (has_name_field) {
    /* Filename editing is explicit.  It never steals the picker's default
     * find-on-type behavior. */
    rect name_area = {content.x, content.y + content.h - name_h, content.w,
                      name_h};
    Render_DrawText(ui->renderer, (v2i){name_area.x, name_area.y}, "File name",
                    ui->font, th->text_muted);
    rect name_input_rect = {name_area.x, name_area.y + 22, name_area.w,
                            name_h - 22};
    UI_PushID("SaveName");
    if (UI_PointInRect(ui->input.mouse_pos, name_input_rect) &&
        ui->input.mouse_pressed[WB_MOUSE_LEFT])
      SaveDialog_SetField(state, ui, WB_SAVE_FIELD_NAME);
    if (state->focused_field == WB_SAVE_FIELD_NAME)
      UI_SetFocus(UI_GenID("Save filename"));
    UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, name_input_rect);
    UI_TextInput(state->name_buffer, sizeof(state->name_buffer),
                 "Save filename", &state->name_input);
    UI_EndLayout();
    UI_PopID();

    if (state->error[0]) {
      Render_DrawText(ui->renderer,
                      (v2i){name_area.x,
                            name_area.y - Font_GetLineHeight(ui->font) - 4},
                      state->error, ui->font, th->error);
    }
  } else if (state->error[0]) {
    Render_DrawText(ui->renderer, (v2i){content.x, content.y + content.h - 4},
                    state->error, ui->font, th->error);
  }

  /* Footer actions. */
  rect footer = shell.footer;
  i32 button_w = 104;
  rect actions = {footer.x + footer.w - (button_w * 2 + th->spacing_md) -
                              shell.outer_pad_x,
                  footer.y + shell.footer_pad_y, button_w * 2 + th->spacing_md,
                  36};
  UI_PushID("SaveActions");
  UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, actions);
  UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, button_w);
  UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, 36);
  if (UI_Button("Cancel"))
    SaveDialog_Finish(state, ui, false);
  UI_PopStyleN(2);
  UI_Spacer(th->spacing_md);
  UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, button_w);
  UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, 36);
  if (state->overwrite_pending) {
    UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, th->warning);
    UI_PushStyleColor(WB_UI_STYLE_TEXT_COLOR, th->background);
  } else {
    UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, th->accent);
  }
  if (UI_Button(state->overwrite_pending ? "Replace" : state->accept_label)) {
    if (!QuickFilter_IsActive(&state->filter))
      SaveDialog_Commit(state, ui);
  }
  if (state->overwrite_pending)
    UI_PopStyle();
  UI_PopStyle();
  UI_PopStyleN(2);
  UI_EndLayout();
  UI_PopID();

  Dialog_EndShell(ui, &shell);
}

b32 SaveDialog_IsOpen(const save_dialog_state *state) {
  return state && state->open;
}

b32 SaveDialog_IsFinished(const save_dialog_state *state) {
  return state && state->finished;
}

b32 SaveDialog_WasAccepted(const save_dialog_state *state) {
  return state && state->finished && state->accepted;
}

const char *SaveDialog_GetResultPath(const save_dialog_state *state) {
  return state ? state->result_path : "";
}

i32 SaveDialog_GetResultCount(const save_dialog_state *state) {
  return state ? state->result_count : 0;
}

const char *SaveDialog_GetResultPathAt(const save_dialog_state *state,
                                       i32 index) {
  if (!state || index < 0 || index >= state->result_count)
    return "";
  return state->result_paths[index];
}
