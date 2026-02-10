/*
 * explorer.c - File Explorer Panel Implementation
 *
 * C99, handmade hero style.
 */

#include "explorer.h"
#include "../../config/config.h"
#include "../../core/fuzzy_match.h"
#include "../../core/input.h"
#include "../../core/text.h"
#include "../../core/theme.h"
#include "../layout.h"
#include "breadcrumb.h"
#include "context_menu.h"
#include "dialog.h"
#include "drag_drop.h"
#include "file_item.h"
#include "quick_filter.h"

#include <stdio.h>
#include <string.h>

/* ===== Configuration ===== */

#define EXPLORER_ITEM_HEIGHT 28
#define EXPLORER_ICON_SIZE 16
#define EXPLORER_ICON_PADDING 6
#define EXPLORER_BREADCRUMB_HEIGHT 32
#define EXPLORER_DOUBLE_CLICK_MS 400
#define EXPLORER_SCROLLBAR_WIDTH 6
#define EXPLORER_SCROLLBAR_GUTTER 12
#define EXPLORER_SCROLLBAR_OFFSET 8

/* ===== Visibility Helpers ===== */

/* Check if an entry at the given index should be visible */
static b32 Explorer_IsEntryVisible(explorer_state *state, i32 index) {
  fs_entry *entry = FS_GetEntry(&state->fs, index);
  if (!entry)
    return false;

  /* ".." is always visible */
  if (strcmp(entry->name, "..") == 0)
    return true;

  /* Hidden files are those starting with '.' */
  if (!state->show_hidden && entry->name[0] == '.') {
    return false;
  }

  /* Apply quick filter if active */
  if (QuickFilter_IsActive(&state->filter)) {
    const char *query = QuickFilter_GetQuery(&state->filter);

    /* Skip filtering for pure navigation prefixes:
     * - Just "~" (navigating to home, show all files)
     * - Just "/" (navigating to root, show all files)
     * - Path ending with separator (user is still typing path)
     */
    if (query[0] == '\0' ||
        (query[0] == '~' && query[1] == '\0') ||
        (query[0] == '/' && query[1] == '\0')) {
      /* Pure navigation prefix - show all (except hidden per above) */
    } else {
      /* Use only the part after the last separator for matching filenames */
      const char *last_sep = FS_FindLastSeparator(query);
      const char *match_query = last_sep ? last_sep + 1 : query;

      /* If query ends with separator, user is still typing path - show all */
      if (match_query[0] != '\0' && !FuzzyMatch(match_query, entry->name)) {
        return false;
      }
    }
  }

  return true;
}

/* Entry with match score for sorting */
typedef struct {
  i32 index;      /* Index into fs.entries */
  i32 score;      /* Fuzzy match score (higher = better) */
} scored_entry;

/* Compare scored entries by score (descending) for qsort */
static int CompareScoredEntries(const void *a, const void *b) {
  const scored_entry *se_a = (const scored_entry *)a;
  const scored_entry *se_b = (const scored_entry *)b;
  return se_b->score - se_a->score; /* Higher score first */
}

/* Update the cached list of visible entries */
static void Explorer_UpdateVisibleEntries(explorer_state *state) {
  state->visible_count = 0;
  
  /* Check if quick filter is active */
  b32 filter_active = QuickFilter_IsActive(&state->filter);
  const char *query = QuickFilter_GetQuery(&state->filter);
  b32 has_query = filter_active && query[0] != '\0';
  
  /* Use stack-allocated array for scored entries when filtering */
  scored_entry scored_entries[FS_MAX_ENTRIES];
  
  for (u32 i = 0; i < state->fs.entry_count; i++) {
    /* Get match score if filter is active */
    i32 score = 0;
    if (has_query) {
      fs_entry *entry = &state->fs.entries[i];
      
      /* Skip filtering for pure navigation prefixes */
      if (query[0] == '\0' ||
          (query[0] == '~' && query[1] == '\0') ||
          (query[0] == '/' && query[1] == '\0')) {
        /* Pure navigation prefix - show all */
      } else {
        /* Use only the part after the last separator for matching filenames */
        const char *last_sep = FS_FindLastSeparator(query);
        const char *match_query = last_sep ? last_sep + 1 : query;
        
        /* If query ends with separator, user is still typing path - show all */
        if (match_query[0] != '\0') {
          fuzzy_match_result result = FuzzyMatchScore(match_query, entry->name);
          if (!result.matches)
            continue;
          score = result.score;
        }
      }
    }
    
    if (Explorer_IsEntryVisible(state, (i32)i)) {
      if (state->visible_count < FS_MAX_ENTRIES) {
        scored_entries[state->visible_count].index = (i32)i;
        scored_entries[state->visible_count].score = score;
        state->visible_count++;
      }
    }
  }
  
  /* Sort by score if filter is active with a query */
  if (has_query && state->visible_count > 1) {
    qsort(scored_entries, (size_t)state->visible_count, sizeof(scored_entry),
          CompareScoredEntries);
  }
  
  /* Copy sorted indices to visible_entries */
  for (i32 i = 0; i < state->visible_count; i++) {
    state->visible_entries[i] = scored_entries[i].index;
  }
}

/* Find the next visible entry index in the given direction */
/* Returns -1 if no visible entry found */
static i32 Explorer_FindNextVisible(explorer_state *state, i32 from,
                                    i32 direction) {
  if (state->visible_count == 0)
    return -1;

  /* Find where 'from' sits in the current visible list */
  i32 visible_idx = -1;
  for (i32 i = 0; i < state->visible_count; i++) {
    if (state->visible_entries[i] == from) {
      visible_idx = i;
      break;
    }
  }

  /* If 'from' is not visible, find the nearest visible entry in preferred
   * direction */
  if (visible_idx == -1) {
    if (direction > 0) {
      for (i32 i = 0; i < state->visible_count; i++) {
        if (state->visible_entries[i] > from)
          return state->visible_entries[i];
      }
    } else {
      for (i32 i = state->visible_count - 1; i >= 0; i--) {
        if (state->visible_entries[i] < from)
          return state->visible_entries[i];
      }
    }
    return -1;
  }

  i32 next_visible_idx = visible_idx + direction;
  if (next_visible_idx >= 0 && next_visible_idx < state->visible_count) {
    return state->visible_entries[next_visible_idx];
  }

  return -1;
}

/* Find the first visible entry */
static i32 Explorer_FindFirstVisible(explorer_state *state) {
  if (state->visible_count > 0) {
    return state->visible_entries[0];
  }
  return 0;
}

/* Find the last visible entry */
static i32 Explorer_FindLastVisible(explorer_state *state) {
  if (state->visible_count > 0) {
    return state->visible_entries[state->visible_count - 1];
  }
  return 0;
}

/* Move selection by delta visible items */
static void Explorer_MoveVisibleSelection(explorer_state *state, i32 delta) {
  i32 direction = delta > 0 ? 1 : -1;
  i32 steps = delta > 0 ? delta : -delta;
  i32 current = state->fs.selected_index;

  for (i32 i = 0; i < steps; i++) {
    i32 next = Explorer_FindNextVisible(state, current, direction);
    if (next < 0)
      break; /* No more visible entries in this direction */
    current = next;
  }

  Explorer_SetSelection(state, current);
}

/* Count visible entries */
static i32 Explorer_CountVisible(explorer_state *state) {
  return state->visible_count;
}

/* Get actual entry index from visible index (for mouse clicks) */
static i32 Explorer_VisibleToActualIndex(explorer_state *state,
                                         i32 visible_index) {
  if (visible_index >= 0 && visible_index < state->visible_count) {
    return state->visible_entries[visible_index];
  }
  return -1;
}

void Explorer_SetSelection(explorer_state *state, i32 index) {
  FS_SetSelection(&state->fs, index);
  SmoothValue_SetTarget(&state->selection_anim, (f32)state->fs.selected_index);
  state->scroll_to_selection = true;
}

/* ===== Initialization ===== */

static void Explorer_ResetScroll(explorer_state *state) {
  state->scroll.scroll_v.current = 0;
  state->scroll.scroll_v.target = 0;
  state->scroll.target_offset.y = 0;
  SmoothValue_SetImmediate(&state->selection_anim,
                           (f32)state->fs.selected_index);
}

void Explorer_Init(explorer_state *state, memory_arena *arena) {
  memset(state, 0, sizeof(*state));

  FS_Init(&state->fs, arena);

  state->item_height = EXPLORER_ITEM_HEIGHT;
  state->show_hidden = Config_GetBool("explorer.show_hidden", false);
  state->show_size_column = true;
  state->show_date_column = false;

  /* Load sort settings */
  const char *sort_type_str = Config_GetString("explorer.sort_type", "name");
  if (strcmp(sort_type_str, "size") == 0)
    state->fs.sort_by = WB_SORT_BY_SIZE;
  else if (strcmp(sort_type_str, "date") == 0)
    state->fs.sort_by = WB_SORT_BY_DATE;
  else
    state->fs.sort_by = WB_SORT_BY_NAME;

  const char *sort_order_str =
      Config_GetString("explorer.sort_order", "ascending");
  if (strcmp(sort_order_str, "descending") == 0)
    state->fs.sort_dir = WB_SORT_DESCENDING;
  else
    state->fs.sort_dir = WB_SORT_ASCENDING;

  /* Initialize scroll container */
  ScrollContainer_Init(&state->scroll);

  /* Selection animation */
  state->selection_anim.speed = 600.0f;

  /* Initialize breadcrumb */
  Breadcrumb_Init(&state->breadcrumb);

  /* Initialize quick filter */
  QuickFilter_Init(&state->filter);

  /* Initialize file system watcher */
  FSWatcher_Init(&state->watcher);

  /* Navigate to home by default */
  FS_NavigateHome(&state->fs);

  /* Start watching the initial directory */
  FSWatcher_WatchDirectory(&state->watcher, state->fs.current_path);

  /* Initialize visible entries */
  Explorer_UpdateVisibleEntries(state);

  /* Initialize history */
  strncpy(state->history[0], state->fs.current_path, FS_MAX_PATH - 1);
  state->history_index = 0;
  state->history_count = 1;
}

void Explorer_Shutdown(explorer_state *state) {
  FSWatcher_Shutdown(&state->watcher);
}

/* ===== Navigation ===== */

b32 Explorer_NavigateTo(explorer_state *state, const char *path,
                        b32 keep_filter) {
  if (FS_PathsEqual(state->fs.current_path, path))
    return true;

  if (FS_LoadDirectory(&state->fs, path)) {
    if (keep_filter) {
      /* Update filter text to match the new location relative to search start
       * Need to use the normalized current_path (which resolves .. and .)
       */
      size_t start_len = strlen(state->search_start_path);
      const char *normalized_path = state->fs.current_path;

      /* Ensure we are still inside the search root */
      if (strncmp(normalized_path, state->search_start_path, start_len) == 0) {
        const char *rel_path = normalized_path + start_len;

        /* Skip leading separator of relative path if present */
        if (FS_IsPathSeparator(rel_path[0])) {
          rel_path++;
        }

        /* Construct new filter text */
        char new_filter[256];
        if (rel_path[0] != '\0') {
          snprintf(new_filter, sizeof(new_filter), "%s/", rel_path);
        } else {
          new_filter[0] = '\0'; /* At root of search */
        }

        QuickFilter_SetBuffer(&state->filter, new_filter);

        /* Do NOT update search_start_path - it acts as the anchor */
      } else {
        /* Navigated outside search root - clear filter */
        QuickFilter_Clear(&state->filter);
      }
    } else {
      /* Clear quick filter on navigation */
      QuickFilter_Clear(&state->filter);
    }

    /* Update file watcher to watch new directory */
    FSWatcher_WatchDirectory(&state->watcher, path);

    /* Push to history */
    Explorer_ResetScroll(state);

    if (state->history_index < EXPLORER_MAX_HISTORY - 1) {
      state->history_index++;
    } else {
      /* Shift history if full */
      for (i32 i = 0; i < EXPLORER_MAX_HISTORY - 1; i++) {
        strncpy(state->history[i], state->history[i + 1], FS_MAX_PATH);
      }
    }

    strncpy(state->history[state->history_index], path, FS_MAX_PATH - 1);
    state->history_count = state->history_index + 1;

    /* Update visible entries cache */
    Explorer_UpdateVisibleEntries(state);

    return true;
  }
  return false;
}

void Explorer_GoBack(explorer_state *state) {
  if (state->history_index > 0) {
    state->history_index--;
    if (FS_LoadDirectory(&state->fs, state->history[state->history_index])) {
      Explorer_ResetScroll(state);
      Explorer_UpdateVisibleEntries(state);
    }
  }
}

void Explorer_GoForward(explorer_state *state) {
  if (state->history_index < state->history_count - 1) {
    state->history_index++;
    if (FS_LoadDirectory(&state->fs, state->history[state->history_index])) {
      Explorer_ResetScroll(state);
      Explorer_UpdateVisibleEntries(state);
    }
  }
}

void Explorer_Refresh(explorer_state *state) {
  i32 old_selection = state->fs.selected_index;
  FS_LoadDirectory(&state->fs, state->fs.current_path);
  Explorer_UpdateVisibleEntries(state);
  Explorer_SetSelection(state, old_selection);
}

fs_entry *Explorer_GetSelected(explorer_state *state) {
  return FS_GetSelectedEntry(&state->fs);
}

void Explorer_ToggleHidden(explorer_state *state) {
  state->show_hidden = !state->show_hidden;
  Config_SetBool("explorer.show_hidden", state->show_hidden);
  Config_Save();
  Explorer_Refresh(state);

  /* If current selection is now hidden, move to a visible entry */
  if (!Explorer_IsEntryVisible(state, state->fs.selected_index)) {
    i32 next = Explorer_FindNextVisible(state, state->fs.selected_index, 1);
    if (next < 0) {
      next = Explorer_FindNextVisible(state, state->fs.selected_index, -1);
    }
    if (next >= 0) {
      Explorer_SetSelection(state, next);
    }
  }
}

/* ===== Dialog Helpers ===== */

static void Explorer_SetupInputDialog(explorer_state *state, explorer_mode mode,
                                      const char *initial_text) {
  state->mode = mode;
  Input_PushFocus(WB_INPUT_TARGET_DIALOG);
  if (initial_text) {
    strncpy(state->input_buffer, initial_text, sizeof(state->input_buffer) - 1);
    state->input_buffer[sizeof(state->input_buffer) - 1] = '\0';
  } else {
    state->input_buffer[0] = '\0';
  }
  memset(&state->input_state, 0, sizeof(state->input_state));
  state->input_state.cursor_pos = (i32)strlen(state->input_buffer);
  state->input_state.has_focus = true;
}

static void Explorer_GetInputPath(explorer_state *state, char *out_path,
                                  size_t out_size) {
  FS_JoinPath(out_path, out_size, state->fs.current_path, state->input_buffer);
}

/* ===== File Operations ===== */

void Explorer_StartRename(explorer_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(&state->fs);
  if (entry && strcmp(entry->name, "..") != 0) {
    Explorer_SetupInputDialog(state, WB_EXPLORER_MODE_RENAME, entry->name);
  }
}

void Explorer_StartCreateFile(explorer_state *state) {
  Explorer_SetupInputDialog(state, WB_EXPLORER_MODE_CREATE_FILE, NULL);
}

void Explorer_StartCreateDir(explorer_state *state) {
  Explorer_SetupInputDialog(state, WB_EXPLORER_MODE_CREATE_DIR, NULL);
}

void Explorer_ConfirmDelete(explorer_state *state, ui_context *ui) {
  /* Count valid deletable items (exclude "..") */
  i32 delete_count = 0;
  for (i32 idx = FS_GetFirstSelected(&state->fs); idx >= 0;
       idx = FS_GetNextSelected(&state->fs, idx)) {
    fs_entry *entry = FS_GetEntry(&state->fs, idx);
    if (entry && strcmp(entry->name, "..") != 0) {
      delete_count++;
    }
  }

  if (delete_count == 0)
    return;

  state->mode = WB_EXPLORER_MODE_CONFIRM_DELETE;
  Input_PushFocus(WB_INPUT_TARGET_DIALOG);

  /* Pre-wrap text using arena */
  const theme *th = ui->theme;

  i32 icon_size = 20;
  i32 text_x = th->spacing_lg + icon_size + th->spacing_md;
  i32 max_text_w = EXPLORER_DIALOG_WIDTH - text_x - th->spacing_lg;

  char msg[512];
  if (delete_count == 1) {
    fs_entry *entry = FS_GetSelectedEntry(&state->fs);
    if (entry && strcmp(entry->name, "..") == 0) {
      /* Find the first non-".." selected item */
      for (i32 idx = FS_GetFirstSelected(&state->fs); idx >= 0;
           idx = FS_GetNextSelected(&state->fs, idx)) {
        fs_entry *e = FS_GetEntry(&state->fs, idx);
        if (e && strcmp(e->name, "..") != 0) {
          entry = e;
          break;
        }
      }
    }
    snprintf(msg, sizeof(msg), "Are you sure you want to delete \"%s\"?",
             entry ? entry->name : "item");
  } else {
    snprintf(msg, sizeof(msg), "Are you sure you want to delete %d items?",
             delete_count);
  }

  state->dialog_text = Text_Wrap(state->fs.arena, msg, ui->font, max_text_w);
}

/* Helper to copy selected items to OS clipboard */
static void Explorer_CopyToClipboard(explorer_state *state, b32 is_cut) {
  const char *paths[EXPLORER_MAX_CLIPBOARD];
  i32 count = 0;

  /* Collect selected paths */
  for (i32 idx = FS_GetFirstSelected(&state->fs);
       idx >= 0 && count < EXPLORER_MAX_CLIPBOARD;
       idx = FS_GetNextSelected(&state->fs, idx)) {
    fs_entry *entry = FS_GetEntry(&state->fs, idx);
    if (entry && strcmp(entry->name, "..") != 0) {
      paths[count] = entry->path;
      count++;
    }
  }

  if (count > 0) {
    Platform_ClipboardSetFiles(paths, count, is_cut);
  }
}

void Explorer_Copy(explorer_state *state) {
  Explorer_CopyToClipboard(state, false);
}

void Explorer_Cut(explorer_state *state) {
  Explorer_CopyToClipboard(state, true);
}

paste_result Explorer_Paste(explorer_state *state) {
  paste_result result = {0};

  /* Get files from OS clipboard */
  char *paths[EXPLORER_MAX_CLIPBOARD];
  char path_buffers[EXPLORER_MAX_CLIPBOARD][FS_MAX_PATH];
  for (i32 i = 0; i < EXPLORER_MAX_CLIPBOARD; i++) {
    paths[i] = path_buffers[i];
  }

  b32 is_cut = false;
  i32 count = Platform_ClipboardGetFiles(paths, EXPLORER_MAX_CLIPBOARD, &is_cut);

  if (count == 0)
    return result;

  for (i32 i = 0; i < count; i++) {
    const char *src = paths[i];
    if (src[0] == '\0')
      continue;

    const char *filename = FS_GetFilename(src);
    char dest[FS_MAX_PATH];
    FS_JoinPath(dest, FS_MAX_PATH, state->fs.current_path, filename);

    b32 success;
    if (is_cut) {
      success = FS_Rename(src, dest);
    } else {
      /* Use recursive copy to handle both files and directories */
      success = FS_CopyRecursive(src, dest, state->fs.arena);
    }

    if (success) {
      result.success_count++;
    } else {
      result.failure_count++;
      snprintf(result.last_error, sizeof(result.last_error), "Failed to %s: %s",
               is_cut ? "move" : "copy", filename);
    }
  }

  /* Show toast/status error if any failures */
  if (result.failure_count > 0) {
    /* TODO: StatusBar_ShowError(result.last_error); */
  }

  if (result.success_count > 0) {
    Explorer_Refresh(state);
  }

  return result;
}

void Explorer_Cancel(explorer_state *state) {
  if (state->mode != WB_EXPLORER_MODE_NORMAL) {
    Input_PopFocus();
  }

  /* Arena-allocated dialog_text is automatically reclaimed */
  state->dialog_text.lines = NULL;
  state->dialog_text.count = 0;

  state->mode = WB_EXPLORER_MODE_NORMAL;
}

void Explorer_FocusFilter(explorer_state *state) {
  QuickFilter_Focus(&state->filter);
}

void Explorer_Duplicate(explorer_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(&state->fs);
  if (entry && strcmp(entry->name, "..") != 0) {
    const char *ext = FS_GetExtension(entry->name);
    i32 name_len = (i32)strlen(entry->name);
    i32 ext_len = (i32)strlen(ext);
    i32 base_len = name_len - ext_len;

    char dest[FS_MAX_PATH];
    char copy_name[FS_MAX_NAME];
    snprintf(copy_name, sizeof(copy_name), "%.*s_copy%s", base_len, entry->name,
             ext);
    FS_JoinPath(dest, sizeof(dest), state->fs.current_path, copy_name);

    if (FS_Copy(entry->path, dest)) {
      Explorer_Refresh(state);
    }
  }
}

void Explorer_OpenSelected(explorer_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(&state->fs);
  if (entry) {
    if (entry->is_directory) {
      char target_path[FS_MAX_PATH];
      strncpy(target_path, entry->path, FS_MAX_PATH - 1);
      target_path[FS_MAX_PATH - 1] = '\0';
      Explorer_NavigateTo(state, target_path,
                          QuickFilter_IsActive(&state->filter));
    } else {
      Platform_OpenFile(entry->path);
    }
  }
}

void Explorer_InvertSelection(explorer_state *state) {
  for (i32 i = 0; i < state->visible_count; i++) {
    i32 actual_index = state->visible_entries[i];
    /* Skip ".." - never toggle it */
    if (strcmp(state->fs.entries[actual_index].name, "..") == 0)
      continue;
    FS_SelectToggle(&state->fs, actual_index);
  }
}

void Explorer_ResetToSingleSelection(explorer_state *state) {
  /* Keep current cursor position but clear multi-selection */
  if (state->fs.entry_count > 0) {
    Explorer_SetSelection(state, state->fs.selected_index);
  } else {
    FS_ClearSelection(&state->fs);
  }
}

/* ===== Input Handling ===== */

static void Explorer_HandleNavigationInput(explorer_state *state,
                                           ui_context *ui, b32 filter_active) {
  ui_input *input = &ui->input;

  /* Vim-style navigation - disabled when filter is active (j/k are printable)
   */
  if (!filter_active && Input_KeyRepeat(WB_KEY_J)) {
    Explorer_MoveVisibleSelection(state, 1);
  }

  if (!filter_active && Input_KeyRepeat(WB_KEY_K)) {
    Explorer_MoveVisibleSelection(state, -1);
  }

  /* Arrow key navigation - always works */
  if (Input_KeyRepeat(WB_KEY_DOWN) && !(input->modifiers & MOD_CTRL)) {
    Explorer_MoveVisibleSelection(state, 1);
  }

  if (Input_KeyRepeat(WB_KEY_UP) && !(input->modifiers & MOD_CTRL)) {
    Explorer_MoveVisibleSelection(state, -1);
  }

  /* Page navigation */
  if (Input_KeyRepeat(WB_KEY_PAGE_DOWN)) {
    i32 visible = (i32)(state->scroll.view_size.y / state->item_height);
    Explorer_MoveVisibleSelection(state, visible);
  }

  if (Input_KeyRepeat(WB_KEY_PAGE_UP)) {
    i32 visible = (i32)(state->scroll.view_size.y / state->item_height);
    Explorer_MoveVisibleSelection(state, -visible);
  }

  /* Home/End */
  if (Input_KeyPressed(WB_KEY_HOME)) {
    Explorer_SetSelection(state, Explorer_FindFirstVisible(state));
  }

  if (Input_KeyPressed(WB_KEY_END)) {
    Explorer_SetSelection(state, Explorer_FindLastVisible(state));
  }

  /* Go home */
  if (Input_KeyPressed(WB_KEY_H) && (input->modifiers & MOD_CTRL)) {
    Explorer_NavigateTo(state, FS_GetHomePath(), false);
  }

  /* History Navigation */
  if ((Input_KeyPressed(WB_KEY_LEFT) && (input->modifiers & MOD_ALT)) ||
      Input_KeyPressed(WB_KEY_BROWSER_BACK) || input->mouse_pressed[WB_MOUSE_X1]) {
    Explorer_GoBack(state);
  }

  if ((Input_KeyPressed(WB_KEY_RIGHT) && (input->modifiers & MOD_ALT)) ||
      Input_KeyPressed(WB_KEY_BROWSER_FORWARD) || input->mouse_pressed[WB_MOUSE_X2]) {
    Explorer_GoForward(state);
  }
}

static void Explorer_HandleOperationInput(explorer_state *state,
                                          ui_context *ui) {
  ui_input *input = &ui->input;

  /* Enter directory or open file */
  if (Input_KeyPressed(WB_KEY_RETURN)) {
    Explorer_OpenSelected(state);
  }

  /* Select all */
  if (Input_KeyPressed(WB_KEY_A) && (input->modifiers & MOD_CTRL)) {
    FS_SelectAll(&state->fs);
  }

  /* Toggle hidden files */
  if (Input_KeyPressed(WB_KEY_PERIOD) && (input->modifiers & MOD_CTRL)) {
    Explorer_ToggleHidden(state);
  }

  /* File operations */
  if (Input_KeyPressed(WB_KEY_R) && (input->modifiers & MOD_CTRL)) {
    Explorer_Refresh(state);
  }

  if (Input_KeyPressed(WB_KEY_F2) ||
      (Input_KeyPressed(WB_KEY_R) && !(input->modifiers & MOD_CTRL))) {
    Explorer_StartRename(state);
  }

  if (Input_KeyPressed(WB_KEY_N) && (input->modifiers & MOD_CTRL)) {
    if (input->modifiers & MOD_SHIFT) {
      Explorer_StartCreateDir(state);
    } else {
      Explorer_StartCreateFile(state);
    }
  }

  if (Input_KeyPressed(WB_KEY_DELETE)) {
    Explorer_ConfirmDelete(state, ui);
  }
}

static void Explorer_HandleClipboardInput(explorer_state *state,
                                          ui_context *ui) {
  ui_input *input = &ui->input;

  if (input->modifiers & MOD_CTRL) {
    if (Input_KeyPressed(WB_KEY_C)) {
      Explorer_Copy(state);
    }

    if (Input_KeyPressed(WB_KEY_X)) {
      Explorer_Cut(state);
    }

    if (Input_KeyPressed(WB_KEY_V)) {
      Explorer_Paste(state);
    }
  }
}

static void HandleNormalInput(explorer_state *state, ui_context *ui) {
  b32 filter_active = QuickFilter_IsActive(&state->filter);

  Explorer_HandleNavigationInput(state, ui, filter_active);
  Explorer_HandleOperationInput(state, ui);
  Explorer_HandleClipboardInput(state, ui);
}

static void Explorer_OnConfirm(explorer_state *state) {
  switch (state->mode) {
  case WB_EXPLORER_MODE_RENAME: {
    fs_entry *entry = FS_GetSelectedEntry(&state->fs);
    if (entry && state->input_buffer[0] != '\0') {
      char new_path[FS_MAX_PATH];
      Explorer_GetInputPath(state, new_path, sizeof(new_path));
      if (FS_Rename(entry->path, new_path)) {
        Explorer_Refresh(state);
      }
    }
    state->mode = WB_EXPLORER_MODE_NORMAL;
  } break;

  case WB_EXPLORER_MODE_CREATE_FILE: {
    if (state->input_buffer[0] != '\0') {
      char new_path[FS_MAX_PATH];
      Explorer_GetInputPath(state, new_path, sizeof(new_path));
      if (FS_CreateFile(new_path)) {
        Explorer_Refresh(state);
      }
    }
    state->mode = WB_EXPLORER_MODE_NORMAL;
  } break;

  case WB_EXPLORER_MODE_CREATE_DIR: {
    if (state->input_buffer[0] != '\0') {
      char new_path[FS_MAX_PATH];
      Explorer_GetInputPath(state, new_path, sizeof(new_path));
      if (FS_CreateDirectory(new_path)) {
        Explorer_Refresh(state);
      }
    }
    state->mode = WB_EXPLORER_MODE_NORMAL;
  } break;

  case WB_EXPLORER_MODE_CONFIRM_DELETE: {
    /* Delete all selected items (skip "..") */
    for (i32 idx = FS_GetFirstSelected(&state->fs); idx >= 0;
         idx = FS_GetNextSelected(&state->fs, idx)) {
      fs_entry *entry = FS_GetEntry(&state->fs, idx);
      if (entry && strcmp(entry->name, "..") != 0) {
        FS_Delete(entry->path, state->fs.arena);
      }
    }
    Explorer_Refresh(state);

    /* Arena-allocated dialog_text is automatically reclaimed */
    state->dialog_text.lines = NULL;
    state->dialog_text.count = 0;

    state->mode = WB_EXPLORER_MODE_NORMAL;
  } break;

  default:
    break;
  }

  if (state->mode == WB_EXPLORER_MODE_NORMAL) {
    Input_PopFocus();
    UI_EndModal();
  }
}

static void HandleDialogInput(explorer_state *state, ui_context *ui) {
  ui_input *input = &ui->input;

  if (input->key_pressed[WB_KEY_ESCAPE]) {
    Explorer_Cancel(state);
    UI_EndModal();
    return;
  }

  if (input->key_pressed[WB_KEY_RETURN]) {
    Explorer_OnConfirm(state);
  }
}

void Explorer_PollWatcher(explorer_state *state) {
  /* Poll file watcher for external changes */
  if (FSWatcher_Poll(&state->watcher)) {
    Explorer_Refresh(state);
  }
}

void Explorer_Update(explorer_state *state, ui_context *ui,
                     drag_drop_state *drag, u32 panel_idx) {
  ui_input *input = &ui->input;

  /* If actively dragging, handle auto-scroll and skip normal input.
   * Note: We only skip when DRAGGING, not when PENDING. PENDING is a transition
   * state where we're waiting to see if the user drags or just clicks.
   * Blocking clicks in PENDING state would break trackpad double-taps. */
  if (DragDrop_IsDragging(drag)) {
    if (input->key_pressed[WB_KEY_ESCAPE]) {
      DragDrop_Cancel(drag);
    }

    /* Auto-scroll when dragging near edges */
    v2i mouse = input->mouse_pos;
    rect bounds = state->list_bounds;
    if (UI_PointInRect(mouse, bounds)) {
      i32 scroll_margin = 35;
      f32 scroll_speed = 500.0f * ui->dt;

      if (mouse.y < bounds.y + scroll_margin) {
        state->scroll.target_offset.y -= scroll_speed;
      } else if (mouse.y > bounds.y + bounds.h - scroll_margin) {
        state->scroll.target_offset.y += scroll_speed;
      }

      /* Clamp target and apply */
      f32 max_scroll = ScrollContainer_GetMaxScroll(&state->scroll);
      if (state->scroll.target_offset.y < 0)
        state->scroll.target_offset.y = 0;
      if (state->scroll.target_offset.y > max_scroll)
        state->scroll.target_offset.y = max_scroll;

      SmoothValue_SetTarget(&state->scroll.scroll_v,
                            state->scroll.target_offset.y);
    }
    return;
  }

  /* Update selection animation */
  SmoothValue_Update(&state->selection_anim, ui->dt);

  /* Update scroll container - handles mouse wheel and scrollbar drag */
  if (state->list_bounds.w > 0 && state->list_bounds.h > 0) {
    ScrollContainer_Update(&state->scroll, ui, state->list_bounds);
  }

  /* Handle mouse input when hovering over list */
  if (state->list_bounds.w > 0 && state->list_bounds.h > 0 &&
      ui->active == UI_ID_NONE && !state->scroll.is_dragging &&
      ui->active_modal == UI_ID_NONE) {
    if (UI_PointInRect(input->mouse_pos, state->list_bounds)) {

      /* Handle mouse click to select items - but not if context menu is open */
      b32 context_menu_open =
          state->context_menu && ContextMenu_IsVisible(state->context_menu);
      b32 mouse_over_menu =
          context_menu_open &&
          ContextMenu_IsMouseOver(state->context_menu, input->mouse_pos);

      if (input->mouse_pressed[WB_MOUSE_LEFT] && !mouse_over_menu) {
        i32 click_y = input->mouse_pos.y - state->list_bounds.y +
                      (i32)state->scroll.offset.y;
        i32 clicked_visible_index = click_y / state->item_height;

        /* Convert visible index to actual entry index */
        i32 actual_index =
            Explorer_VisibleToActualIndex(state, clicked_visible_index);

        if (actual_index >= 0) {
          /* Check for double-click */
          u64 now = Platform_GetTimeMs();
          if (actual_index == state->last_click_index &&
              (now - state->last_click_time) < EXPLORER_DOUBLE_CLICK_MS) {
            /* Double-click - navigate into */
            Explorer_SetSelection(state, actual_index);
            fs_entry *entry = FS_GetSelectedEntry(&state->fs);
            if (entry) {
              if (entry->is_directory) {
                char target_path[FS_MAX_PATH];
                strncpy(target_path, entry->path, FS_MAX_PATH - 1);
                target_path[FS_MAX_PATH - 1] = '\0';
                Explorer_NavigateTo(state, target_path,
                                    QuickFilter_IsActive(&state->filter));
              } else {
                Platform_OpenFile(entry->path);
              }
            }
            state->last_click_time = 0;
          } else {
            /* Single click - select with multi-select support */
            /* Check if item is already selected BEFORE modifying selection */
            b32 was_already_selected = FS_IsSelected(&state->fs, actual_index);

            if (input->modifiers & MOD_CTRL) {
              FS_SelectToggle(&state->fs, actual_index);
            } else if (input->modifiers & MOD_SHIFT) {
              i32 anchor = state->fs.selection_anchor;
              if (anchor < 0)
                anchor = state->fs.selected_index;
              if (anchor < 0)
                anchor = 0;
              FS_SelectRange(&state->fs, anchor, actual_index);
            } else if (!was_already_selected) {
              /* Only reset selection if clicking on unselected item.
               * If clicking on already-selected item, preserve multi-selection
               * to allow dragging multiple items. */
              Explorer_SetSelection(state, actual_index);
            }
            state->last_click_time = now;
            state->last_click_index = actual_index;

            /* Start potential drag - only on single clicks, not double-clicks
             */
            fs_entry *entry = FS_GetEntry(&state->fs, actual_index);
            if (entry && strcmp(entry->name, "..") != 0 &&
                FS_IsSelected(&state->fs, actual_index)) {
              DragDrop_BeginPotential(drag, &state->fs, actual_index, panel_idx,
                                      input->mouse_pos, now);
            }
          }
        } else {
          /* Clicked on empty space - clear selection */
          FS_ClearSelection(&state->fs);
          state->fs.selected_index = -1;
        }
      }

      /* Handle right-click for context menu */
      if (input->mouse_pressed[WB_MOUSE_RIGHT] && state->context_menu) {
        i32 click_y = input->mouse_pos.y - state->list_bounds.y +
                      (i32)state->scroll.offset.y;
        i32 clicked_visible_index = click_y / state->item_height;

        /* Convert visible index to actual entry index */
        i32 actual_index =
            Explorer_VisibleToActualIndex(state, clicked_visible_index);

        if (actual_index >= 0) {
          /* Only change selection if clicking on unselected item */
          if (!FS_IsSelected(&state->fs, actual_index)) {
            Explorer_SetSelection(state, actual_index);
          }

          /* Determine context type and show menu */
          fs_entry *entry = FS_GetEntry(&state->fs, actual_index);
          if (entry) {
            context_type type =
                entry->is_directory ? WB_CONTEXT_DIRECTORY : WB_CONTEXT_FILE;
            ContextMenu_Show(state->context_menu, input->mouse_pos, type,
                             entry->path, state, ui);
          }
        } else {
          /* Right-click on empty space */
          ContextMenu_Show(state->context_menu, input->mouse_pos, WB_CONTEXT_EMPTY,
                           state->fs.current_path, state, ui);
        }
      }
    }
  }

  if (state->mode == WB_EXPLORER_MODE_NORMAL) {
    /* Update quick filter first - it may consume input */
    /* Skip quick filter when breadcrumb is being edited */
    b32 filter_was_active = state->filter_was_active;
    b32 filter_is_active = state->filter_was_active;
    if (!state->breadcrumb.is_editing) {
      QuickFilter_Update(&state->filter, ui);
      filter_is_active = QuickFilter_IsActive(&state->filter);
    }

    state->filter_was_active = filter_is_active;

    if (!filter_was_active && filter_is_active) {
      /* Filter just started - save current path */
      strncpy(state->search_start_path, state->fs.current_path,
              FS_MAX_PATH - 1);
      state->search_start_path[FS_MAX_PATH - 1] = '\0';
    } else if (filter_is_active && !state->breadcrumb.is_editing) {
      /* Filter is active - handle folder traversal */
      /* Skip when breadcrumb is editing to avoid conflicts */
      const char *query = QuickFilter_GetQuery(&state->filter);
      if (query) {
        /* Check for special root/home navigation triggers */
        if (query[0] == '/' && strlen(query) == 1) {
          /* User typed just "/" - navigate to root */
          if (!FS_PathsEqual(state->fs.current_path, "/")) {
            if (FS_LoadDirectory(&state->fs, "/")) {
              /* Update search root to root directory */
              strncpy(state->search_start_path, "/", FS_MAX_PATH - 1);
              state->search_start_path[FS_MAX_PATH - 1] = '\0';
              state->scroll.scroll_v.current = 0;
              state->scroll.scroll_v.target = 0;
              state->scroll.target_offset.y = 0;
            }
          }
        } else if (query[0] == '~' && (query[1] == '\0' || query[1] == '/')) {
          /* User typed "~" or "~/" - navigate to home */
          const char *home = FS_GetHomePath();
          if (!FS_PathsEqual(state->fs.current_path, home)) {
            if (FS_LoadDirectory(&state->fs, home)) {
              /* Update search root to home directory */
              strncpy(state->search_start_path, home, FS_MAX_PATH - 1);
              state->search_start_path[FS_MAX_PATH - 1] = '\0';
              state->scroll.scroll_v.current = 0;
              state->scroll.scroll_v.target = 0;
              state->scroll.target_offset.y = 0;
            }
          }
        } else {
          /* Extract path part from query (everything up to last separator) */
          char path_part[FS_MAX_PATH] = {0};
          const char *last_sep = FS_FindLastSeparator(query);

          if (last_sep) {
            size_t len = (size_t)(last_sep - query) + 1;
            if (len < sizeof(path_part)) {
              memcpy(path_part, query, len);
              path_part[len] = '\0';
            }
          }

          /* Construct target path: search_start_path + path_part */
          char target_path[FS_MAX_PATH] = {0};

          /* Always start from the search start location */
          if (path_part[0] != '\0') {
            FS_JoinPath(target_path, FS_MAX_PATH, state->search_start_path,
                        path_part);
          } else {
            strncpy(target_path, state->search_start_path, FS_MAX_PATH - 1);
            target_path[FS_MAX_PATH - 1] = '\0';
          }

          /* Strip trailing slash from target_path for comparison and loading
             (unless it is just root "/") */
          size_t target_len = strlen(target_path);
          if (target_len > 1 && target_path[target_len - 1] == '/') {
            target_path[target_len - 1] = '\0';
          }

          /* If target path is different from current, navigate */
          if (target_path[0] != '\0' &&
              !FS_PathsEqual(state->fs.current_path, target_path)) {
            /* Use FS_LoadDirectory directly to avoid history push and filter
             * clear */
            if (FS_LoadDirectory(&state->fs, target_path)) {
              Explorer_ResetScroll(state);
            }
          }
        }
      }
    } else if (filter_was_active && !filter_is_active) {
      /* Filter just ended - stay in current directory (do not revert) */
    }

    /* If filter state or content changed, reset selection */
    const char *current_filter = QuickFilter_GetQuery(&state->filter);
    if (filter_was_active != filter_is_active ||
        strcmp(state->last_filter_buffer, current_filter) != 0) {

      /* Update last buffer */
      strncpy(state->last_filter_buffer, current_filter,
              sizeof(state->last_filter_buffer) - 1);
      state->last_filter_buffer[sizeof(state->last_filter_buffer) - 1] = '\0';

      /* Update visible entries when filter changes */
      Explorer_UpdateVisibleEntries(state);

      /* When filter has a query, select the first visible entry (best match after
         sorting by score). Otherwise (no query), skip ".." and select first real
         match if available. */
      i32 target = -1;

      if (filter_is_active && current_filter[0] != '\0') {
        /* Filter has query - select first entry (best fuzzy match) */
        if (state->visible_count > 0) {
          target = state->visible_entries[0];
        }
      } else {
        /* No query - old behavior: skip ".." if there's a second item */
        if (state->visible_count > 1) {
          target = state->visible_entries[1];
        } else if (state->visible_count > 0) {
          target = state->visible_entries[0];
        }
      }

      if (target >= 0) {
        Explorer_SetSelection(state, target);
      }
    }

    /* Skip normal explorer input when breadcrumb is being edited */
    if (!state->breadcrumb.is_editing) {
      HandleNormalInput(state, ui);
    }
  } else {
    UI_BeginModal("ExplorerDialog");
    HandleDialogInput(state, ui);
  }
}

/* ===== Rendering ===== */

static void RenderDialog(explorer_state *state, ui_context *ui, rect bounds) {
  /* Build dialog configuration based on current mode */
  dialog_config config = {0};

  switch (state->mode) {
  case WB_EXPLORER_MODE_RENAME:
    config.type = WB_DIALOG_TYPE_INPUT;
    config.title = "Rename";
    config.input_buffer = state->input_buffer;
    config.input_buffer_size = sizeof(state->input_buffer);
    config.input_state = &state->input_state;
    config.placeholder = "Enter new name...";
    break;
  case WB_EXPLORER_MODE_CREATE_FILE:
    config.type = WB_DIALOG_TYPE_INPUT;
    config.title = "New File";
    config.input_buffer = state->input_buffer;
    config.input_buffer_size = sizeof(state->input_buffer);
    config.input_state = &state->input_state;
    config.placeholder = "Enter filename...";
    break;
  case WB_EXPLORER_MODE_CREATE_DIR:
    config.type = WB_DIALOG_TYPE_INPUT;
    config.title = "New Folder";
    config.input_buffer = state->input_buffer;
    config.input_buffer_size = sizeof(state->input_buffer);
    config.input_state = &state->input_state;
    config.placeholder = "Enter folder name...";
    break;
  case WB_EXPLORER_MODE_CONFIRM_DELETE:
    config.type = WB_DIALOG_TYPE_CONFIRM;
    config.title = "Delete?";
    config.is_danger = true;
    config.message = state->dialog_text;
    config.hint = "This action cannot be undone.";
    config.confirm_label = "Delete";
    break;
  default:
    return;
  }

  dialog_result result = Dialog_Render(ui, bounds, &config);

  if (result == WB_DIALOG_RESULT_CONFIRM) {
    Explorer_OnConfirm(state);
  } else if (result == WB_DIALOG_RESULT_CANCEL) {
    Explorer_Cancel(state);
    UI_EndModal();
  }
}

void Explorer_Render(explorer_state *state, ui_context *ui, rect bounds,
                     b32 has_focus, drag_drop_state *drag, u32 panel_idx) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;

  /* Background */
  Render_DrawRect(ctx, bounds, th->panel_alt);

  /* Focus Indicator */
  if (has_focus) {
    /* Draw a subtle accent border */
    rect border = bounds;
    Render_DrawRectRounded(ctx, border, 0, th->accent);

    /* Inset content slightly */
    rect inner = {bounds.x + 2, bounds.y + 2, bounds.w - 4, bounds.h - 4};
    Render_DrawRect(ctx, inner, th->panel_alt);

    /* Adjust bounds for child content */
    /* Note: We don't actually change 'bounds' passed to children because
       we want the scrollbar/etc to still sit flush, but visually the border is
       on top/under? Actually, let's just draw the border frame. */
  }

  /* Breadcrumb at top */
  rect breadcrumb = {bounds.x, bounds.y, bounds.w, EXPLORER_BREADCRUMB_HEIGHT};
  breadcrumb_result bc_result =
      Breadcrumb_Render(ui, breadcrumb, state->fs.current_path, &state->breadcrumb);

  /* Handle breadcrumb navigation */
  if (bc_result.clicked_segment >= 0) {
    char target_path[FS_MAX_PATH];
    if (Breadcrumb_GetPathForSegment(state->fs.current_path,
                                      bc_result.clicked_segment, target_path,
                                      sizeof(target_path))) {
      Explorer_NavigateTo(state, target_path,
                          QuickFilter_IsActive(&state->filter));
    }
  }

  /* Handle breadcrumb editing results - navigate on text change (once per
   * character) or when Enter is pressed */
  if (bc_result.text_changed || bc_result.editing_finished) {
    /* Resolve the path (handle ~ expansion, etc.) */
    char resolved_path[FS_MAX_PATH];
    if (FS_ResolvePath(state->breadcrumb.edit_buffer, resolved_path,
                       sizeof(resolved_path))) {
      /* Try to navigate to the path. If it fails, find the deepest valid
       * parent directory using the FS helper. */
      if (!Explorer_NavigateTo(state, resolved_path, false)) {
        /* Path doesn't exist - find the deepest valid parent directory */
        char deepest_valid[FS_MAX_PATH];
        if (FS_FindDeepestValidDirectory(resolved_path, deepest_valid,
                                          sizeof(deepest_valid))) {
          Explorer_NavigateTo(state, deepest_valid, false);
        }
      }
    }
  } else if (bc_result.editing_cancelled) {
    /* User pressed Escape - just exit edit mode, no navigation */
  }

  /* File list area */
  rect list_area = {bounds.x, bounds.y + EXPLORER_BREADCRUMB_HEIGHT, bounds.w,
                    bounds.h - EXPLORER_BREADCRUMB_HEIGHT};

  /* Store bounds for mouse input handling */
  state->list_bounds = list_area;

  /* Count visible items first for proper calculations */
  i32 visible_item_count = Explorer_CountVisible(state);
  f32 content_height = (f32)(visible_item_count * state->item_height);

  /* Add extra padding (3 items) when scrollbar is needed so user can scroll
   * past the end to access empty space for right-clicking */
  if (content_height > list_area.h) {
    content_height += (f32)(3 * state->item_height);
  }

  /* Update scroll container content size */
  ScrollContainer_SetContentSize(&state->scroll, content_height);

  /* Ensure selection is visible - only when navigation triggered it */
  if (state->scroll_to_selection) {
    /* Calculate the visible index of the selected entry or closest visible
     * entry
     */
    i32 visible_sel_index = 0;
    for (i32 i = 0; i < state->visible_count; i++) {
      if (state->visible_entries[i] >= state->fs.selected_index) {
        visible_sel_index = i;
        break;
      }
      visible_sel_index = i;
    }

    f32 sel_y = (f32)(visible_sel_index * state->item_height);
    ScrollContainer_ScrollToY(&state->scroll, sel_y, (f32)state->item_height);
    state->scroll_to_selection = false;
  }

  /* Set clip rect for list */
  Render_SetClipRect(ctx, list_area);

  /* Check focus state to disable hover when modals are active */
  input_target focus = Input_GetFocus();
  b32 modal_active =
      (focus == WB_INPUT_TARGET_COMMAND_PALETTE || focus == WB_INPUT_TARGET_DIALOG ||
       focus == WB_INPUT_TARGET_CONTEXT_MENU);

  /* Draw visible items - iterate only through items in the viewport using
   * cached indices */
  i32 start_visible = (i32)(state->scroll.offset.y / state->item_height);
  i32 end_visible = start_visible + list_area.h / state->item_height + 2;

  if (start_visible < 0)
    start_visible = 0;
  if (end_visible > state->visible_count)
    end_visible = state->visible_count;

  for (i32 i = start_visible; i < end_visible; i++) {
    i32 actual_index = state->visible_entries[i];
    fs_entry *entry = FS_GetEntry(&state->fs, actual_index);
    if (!entry)
      continue;

    i32 item_y =
        list_area.y + (i * state->item_height) - (i32)state->scroll.offset.y;
    i32 actual_width = list_area.w;
    if (state->scroll.content_size.y > state->scroll.view_size.y) {
      actual_width -=
          EXPLORER_SCROLLBAR_GUTTER; /* Reserve space for scrollbar */
    }
    rect item_bounds = {list_area.x, item_y, actual_width, state->item_height};

    /* Check if this folder is a drop target */
    if (DragDrop_IsDragging(drag) && entry->is_directory) {
      DragDrop_CheckTarget(drag, entry, item_bounds, panel_idx);

      /* Render highlight if this is the current target */
      if (drag->target_type != WB_DROP_TARGET_NONE &&
          drag->target_panel_idx == panel_idx &&
          drag->target_bounds.x == item_bounds.x &&
          drag->target_bounds.y == item_bounds.y) {
        DragDrop_RenderTargetHighlight(drag, ui, item_bounds);
      }
    }

    b32 is_selected = FS_IsSelected(&state->fs, actual_index);
    b32 is_hovered = !modal_active && (ui->active == UI_ID_NONE) &&
                     UI_PointInRect(ui->input.mouse_pos, item_bounds);
    file_item_config item_config = {.icon_size = EXPLORER_ICON_SIZE,
                                    .icon_padding = EXPLORER_ICON_PADDING,
                                    .show_size = state->show_size_column};
    FileItem_Render(ui, entry, item_bounds, is_selected, is_hovered,
                    &item_config);
  }

  /* Check panel itself as drop target (empty area) */
  DragDrop_CheckPanelTarget(drag, state->fs.current_path, list_area, panel_idx);
  if (drag->target_type == WB_DROP_TARGET_PANEL &&
      drag->target_panel_idx == panel_idx) {
    DragDrop_RenderTargetHighlight(drag, ui, list_area);
  }

  /* Reset clip */
  Render_ResetClipRect(ctx);

  /* Draw scrollbar */
  ScrollContainer_RenderScrollbar(&state->scroll, ui);

  /* Draw quick filter overlay (positioned at bottom of list area) */
  QuickFilter_Render(&state->filter, ui, list_area);

  /* Draw dialog overlay if in a mode */
  if (state->mode != WB_EXPLORER_MODE_NORMAL) {
    RenderDialog(state, ui, bounds);
  }
}
