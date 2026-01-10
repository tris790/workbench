/*
 * explorer.c - File Explorer Panel Implementation
 *
 * C99, handmade hero style.
 */

#include "explorer.h"
#include "icons.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

/* ===== Configuration ===== */

#define EXPLORER_ITEM_HEIGHT 28
#define EXPLORER_ICON_SIZE 16
#define EXPLORER_ICON_PADDING 6
#define EXPLORER_BREADCRUMB_HEIGHT 32
#define EXPLORER_DOUBLE_CLICK_MS 400

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

  return true;
}

/* Find the next visible entry index in the given direction */
/* Returns -1 if no visible entry found */
static i32 Explorer_FindNextVisible(explorer_state *state, i32 from,
                                    i32 direction) {
  i32 index = from + direction;

  while (index >= 0 && (u32)index < state->fs.entry_count) {
    if (Explorer_IsEntryVisible(state, index)) {
      return index;
    }
    index += direction;
  }

  return -1; /* No visible entry found */
}

/* Find the first visible entry */
static i32 Explorer_FindFirstVisible(explorer_state *state) {
  for (i32 i = 0; (u32)i < state->fs.entry_count; i++) {
    if (Explorer_IsEntryVisible(state, i)) {
      return i;
    }
  }
  return 0;
}

/* Find the last visible entry */
static i32 Explorer_FindLastVisible(explorer_state *state) {
  for (i32 i = (i32)state->fs.entry_count - 1; i >= 0; i--) {
    if (Explorer_IsEntryVisible(state, i)) {
      return i;
    }
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

  FS_SetSelection(&state->fs, current);
}

/* Count visible entries */
static i32 Explorer_CountVisible(explorer_state *state) {
  i32 count = 0;
  for (u32 i = 0; i < state->fs.entry_count; i++) {
    if (Explorer_IsEntryVisible(state, (i32)i)) {
      count++;
    }
  }
  return count;
}

/* Get actual entry index from visible index (for mouse clicks) */
static i32 Explorer_VisibleToActualIndex(explorer_state *state,
                                         i32 visible_index) {
  i32 visible_count = 0;
  for (u32 i = 0; i < state->fs.entry_count; i++) {
    if (Explorer_IsEntryVisible(state, (i32)i)) {
      if (visible_count == visible_index) {
        return (i32)i;
      }
      visible_count++;
    }
  }
  return -1; /* Not found */
}

/* ===== Initialization ===== */

void Explorer_Init(explorer_state *state, memory_arena *arena) {
  memset(state, 0, sizeof(*state));

  FS_Init(&state->fs, arena);

  state->item_height = EXPLORER_ITEM_HEIGHT;
  state->show_hidden = false;
  state->show_size_column = true;
  state->show_date_column = false;

  /* Smooth scroll settings - use high speed for responsive feel */
  state->scroll.scroll_v.speed = 1500.0f;
  state->scroll.scroll_h.speed = 1500.0f;

  /* Selection animation */
  state->selection_anim.speed = 600.0f;

  /* Navigate to home by default */
  FS_NavigateHome(&state->fs);
  
  /* Initialize history */
  strncpy(state->history[0], state->fs.current_path, FS_MAX_PATH - 1);
  state->history_index = 0;
  state->history_count = 1;
}

/* ===== Navigation ===== */

b32 Explorer_NavigateTo(explorer_state *state, const char *path) {
  if (strcmp(state->fs.current_path, path) == 0)
    return true;

  if (FS_LoadDirectory(&state->fs, path)) {
    /* Push to history */
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
    return true;
  }
  return false;
}

void Explorer_GoBack(explorer_state *state) {
  if (state->history_index > 0) {
    state->history_index--;
    FS_LoadDirectory(&state->fs, state->history[state->history_index]);
  }
}

void Explorer_GoForward(explorer_state *state) {
  if (state->history_index < state->history_count - 1) {
    state->history_index++;
    FS_LoadDirectory(&state->fs, state->history[state->history_index]);
  }
}

void Explorer_Refresh(explorer_state *state) {
  i32 old_selection = state->fs.selected_index;
  FS_LoadDirectory(&state->fs, state->fs.current_path);
  FS_SetSelection(&state->fs, old_selection);
}

fs_entry *Explorer_GetSelected(explorer_state *state) {
  return FS_GetSelectedEntry(&state->fs);
}

void Explorer_ToggleHidden(explorer_state *state) {
  state->show_hidden = !state->show_hidden;
  Explorer_Refresh(state);

  /* If current selection is now hidden, move to a visible entry */
  if (!Explorer_IsEntryVisible(state, state->fs.selected_index)) {
    i32 next = Explorer_FindNextVisible(state, state->fs.selected_index, 1);
    if (next < 0) {
      next = Explorer_FindNextVisible(state, state->fs.selected_index, -1);
    }
    if (next >= 0) {
      FS_SetSelection(&state->fs, next);
    }
  }
}

/* ===== File Operations ===== */

void Explorer_StartRename(explorer_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(&state->fs);
  if (entry && strcmp(entry->name, "..") != 0) {
    state->mode = EXPLORER_MODE_RENAME;
    strncpy(state->input_buffer, entry->name, sizeof(state->input_buffer) - 1);
    memset(&state->input_state, 0, sizeof(state->input_state));
    state->input_state.cursor_pos = (i32)strlen(state->input_buffer);
  }
}

void Explorer_StartCreateFile(explorer_state *state) {
  state->mode = EXPLORER_MODE_CREATE_FILE;
  state->input_buffer[0] = '\0';
  memset(&state->input_state, 0, sizeof(state->input_state));
}

void Explorer_StartCreateDir(explorer_state *state) {
  state->mode = EXPLORER_MODE_CREATE_DIR;
  state->input_buffer[0] = '\0';
  memset(&state->input_state, 0, sizeof(state->input_state));
}

void Explorer_ConfirmDelete(explorer_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(&state->fs);
  if (entry && strcmp(entry->name, "..") != 0) {
    state->mode = EXPLORER_MODE_CONFIRM_DELETE;
  }
}

void Explorer_Copy(explorer_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(&state->fs);
  if (entry && strcmp(entry->name, "..") != 0) {
    strncpy(state->clipboard_path, entry->path, FS_MAX_PATH - 1);
    state->clipboard_is_cut = false;
  }
}

void Explorer_Cut(explorer_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(&state->fs);
  if (entry && strcmp(entry->name, "..") != 0) {
    strncpy(state->clipboard_path, entry->path, FS_MAX_PATH - 1);
    state->clipboard_is_cut = true;
  }
}

void Explorer_Paste(explorer_state *state) {
  if (state->clipboard_path[0] == '\0')
    return;

  const char *filename = FS_GetFilename(state->clipboard_path);
  char dest[FS_MAX_PATH];
  FS_JoinPath(dest, FS_MAX_PATH, state->fs.current_path, filename);

  if (state->clipboard_is_cut) {
    if (FS_Rename(state->clipboard_path, dest)) {
      state->clipboard_path[0] = '\0';
      Explorer_Refresh(state);
    }
  } else {
    if (FS_Copy(state->clipboard_path, dest)) {
      Explorer_Refresh(state);
    }
  }
}

void Explorer_Cancel(explorer_state *state) {
  state->mode = EXPLORER_MODE_NORMAL;
}

/* ===== Input Handling ===== */

static void HandleNormalInput(explorer_state *state, ui_context *ui) {
  ui_input *input = &ui->input;

  /* Vim-style navigation */
  if (input->key_pressed[KEY_J] ||
      (input->key_pressed[KEY_DOWN] && !(input->modifiers & MOD_CTRL))) {
    Explorer_MoveVisibleSelection(state, 1);
    SmoothValue_SetTarget(&state->selection_anim,
                          (f32)state->fs.selected_index);
    state->scroll_to_selection = true;
  }

  if (input->key_pressed[KEY_K] ||
      (input->key_pressed[KEY_UP] && !(input->modifiers & MOD_CTRL))) {
    Explorer_MoveVisibleSelection(state, -1);
    SmoothValue_SetTarget(&state->selection_anim,
                          (f32)state->fs.selected_index);
    state->scroll_to_selection = true;
  }

  /* Page navigation */
  if (input->key_pressed[KEY_PAGE_DOWN]) {
    i32 visible = (i32)(state->scroll.view_size.y / state->item_height);
    Explorer_MoveVisibleSelection(state, visible);
    state->scroll_to_selection = true;
  }

  if (input->key_pressed[KEY_PAGE_UP]) {
    i32 visible = (i32)(state->scroll.view_size.y / state->item_height);
    Explorer_MoveVisibleSelection(state, -visible);
    state->scroll_to_selection = true;
  }

  /* Home/End */
  if (input->key_pressed[KEY_HOME]) {
    FS_SetSelection(&state->fs, Explorer_FindFirstVisible(state));
    state->scroll_to_selection = true;
  }

  if (input->key_pressed[KEY_END]) {
    FS_SetSelection(&state->fs, Explorer_FindLastVisible(state));
    state->scroll_to_selection = true;
  }

  /* Enter directory or open file */
  if (input->key_pressed[KEY_RETURN]) {
    fs_entry *entry = FS_GetSelectedEntry(&state->fs);
    if (entry) {
      if (entry->is_directory) {
        char target_path[FS_MAX_PATH];
        strncpy(target_path, entry->path, FS_MAX_PATH - 1);
        target_path[FS_MAX_PATH - 1] = '\0';
        Explorer_NavigateTo(state, target_path);
      } else {
        Platform_OpenFile(entry->path);
      }
    }
  }

  /* Go back (Up directory) */
  if (input->key_pressed[KEY_BACKSPACE]) {
    char parent_path[FS_MAX_PATH];
    FS_JoinPath(parent_path, FS_MAX_PATH, state->fs.current_path, "..");
    Explorer_NavigateTo(state, parent_path);
  }

  /* Go home */
  if (input->key_pressed[KEY_H] && (input->modifiers & MOD_CTRL)) {
    Explorer_NavigateTo(state, FS_GetHomePath());
  }

  /* History Navigation */
  if ((input->key_pressed[KEY_LEFT] && (input->modifiers & MOD_ALT)) ||
      input->mouse_pressed[MOUSE_X1]) {
    Explorer_GoBack(state);
  }

  if ((input->key_pressed[KEY_RIGHT] && (input->modifiers & MOD_ALT)) ||
      input->mouse_pressed[MOUSE_X2]) {
    Explorer_GoForward(state);
  }

  /* Toggle hidden files */
  if (input->key_pressed[KEY_PERIOD] && (input->modifiers & MOD_CTRL)) {
    Explorer_ToggleHidden(state);
  }

  /* File operations */
  if (input->key_pressed[KEY_R] && (input->modifiers & MOD_CTRL)) {
    Explorer_Refresh(state);
  }

  if (input->key_pressed[KEY_F2] ||
      (input->key_pressed[KEY_R] && !(input->modifiers & MOD_CTRL))) {
    Explorer_StartRename(state);
  }

  if (input->key_pressed[KEY_N] && (input->modifiers & MOD_CTRL)) {
    if (input->modifiers & MOD_SHIFT) {
      Explorer_StartCreateDir(state);
    } else {
      Explorer_StartCreateFile(state);
    }
  }

  if (input->key_pressed[KEY_DELETE]) {
    Explorer_ConfirmDelete(state);
  }

  if (input->key_pressed[KEY_C] && (input->modifiers & MOD_CTRL)) {
    Explorer_Copy(state);
  }

  if (input->key_pressed[KEY_X] && (input->modifiers & MOD_CTRL)) {
    Explorer_Cut(state);
  }

  if (input->key_pressed[KEY_V] && (input->modifiers & MOD_CTRL)) {
    Explorer_Paste(state);
  }
}

static void HandleDialogInput(explorer_state *state, ui_context *ui) {
  ui_input *input = &ui->input;

  if (input->key_pressed[KEY_ESCAPE]) {
    Explorer_Cancel(state);
    UI_EndModal();
    return;
  }

  if (input->key_pressed[KEY_RETURN]) {
    switch (state->mode) {
    case EXPLORER_MODE_RENAME: {
      fs_entry *entry = FS_GetSelectedEntry(&state->fs);
      if (entry && state->input_buffer[0] != '\0') {
        char new_path[FS_MAX_PATH];
        FS_JoinPath(new_path, FS_MAX_PATH, state->fs.current_path,
                    state->input_buffer);
        if (FS_Rename(entry->path, new_path)) {
          Explorer_Refresh(state);
        }
      }
      state->mode = EXPLORER_MODE_NORMAL;
    } break;

    case EXPLORER_MODE_CREATE_FILE: {
      if (state->input_buffer[0] != '\0') {
        char new_path[FS_MAX_PATH];
        FS_JoinPath(new_path, FS_MAX_PATH, state->fs.current_path,
                    state->input_buffer);
        if (FS_CreateFile(new_path)) {
          Explorer_Refresh(state);
        }
      }
      state->mode = EXPLORER_MODE_NORMAL;
    } break;

    case EXPLORER_MODE_CREATE_DIR: {
      if (state->input_buffer[0] != '\0') {
        char new_path[FS_MAX_PATH];
        FS_JoinPath(new_path, FS_MAX_PATH, state->fs.current_path,
                    state->input_buffer);
        if (FS_CreateDirectory(new_path)) {
          Explorer_Refresh(state);
        }
      }
      state->mode = EXPLORER_MODE_NORMAL;
    } break;

    case EXPLORER_MODE_CONFIRM_DELETE: {
      fs_entry *entry = FS_GetSelectedEntry(&state->fs);
      if (entry) {
        if (FS_Delete(entry->path)) {
          Explorer_Refresh(state);
        }
      }
      state->mode = EXPLORER_MODE_NORMAL;
    } break;

    default:
      break;
    }
    
    /* Close modal if returning to normal */
    if (state->mode == EXPLORER_MODE_NORMAL) {
       UI_EndModal();
    }
  }
}

void Explorer_Update(explorer_state *state, ui_context *ui) {
  ui_input *input = &ui->input;

  /* Update animations */
  SmoothValue_Update(&state->selection_anim, ui->dt);
  SmoothValue_Update(&state->scroll.scroll_v, ui->dt);
  state->scroll.offset.y = state->scroll.scroll_v.current;

  /* Handle mouse scroll when hovering over list */
  if (state->list_bounds.w > 0 && state->list_bounds.h > 0) {
    if (UI_PointInRect(input->mouse_pos, state->list_bounds)) {
      if (input->scroll_delta != 0) {
        state->scroll.target_offset.y -= input->scroll_delta * 80.0f;

        /* Clamp scroll */
        f32 max_scroll =
            state->scroll.content_size.y - state->scroll.view_size.y;
        if (max_scroll < 0)
          max_scroll = 0;
        if (state->scroll.target_offset.y < 0)
          state->scroll.target_offset.y = 0;
        if (state->scroll.target_offset.y > max_scroll)
          state->scroll.target_offset.y = max_scroll;

        SmoothValue_SetTarget(&state->scroll.scroll_v,
                              state->scroll.target_offset.y);
      }

      /* Handle mouse click to select items */
      if (input->mouse_pressed[MOUSE_LEFT]) {
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
            FS_SetSelection(&state->fs, actual_index);
            fs_entry *entry = FS_GetSelectedEntry(&state->fs);
            if (entry) {
              if (entry->is_directory) {
                char target_path[FS_MAX_PATH];
                strncpy(target_path, entry->path, FS_MAX_PATH - 1);
                target_path[FS_MAX_PATH - 1] = '\0';
                Explorer_NavigateTo(state, target_path);
              } else {
                Platform_OpenFile(entry->path);
              }
            }
            state->last_click_time = 0;
          } else {
            /* Single click - select */
            FS_SetSelection(&state->fs, actual_index);
            state->last_click_time = now;
            state->last_click_index = actual_index;
          }
        }
      }
    }
  }

  if (state->mode == EXPLORER_MODE_NORMAL) {
    HandleNormalInput(state, ui);
  } else {
    UI_BeginModal("ExplorerDialog");
    HandleDialogInput(state, ui);
  }
}

/* ===== Rendering ===== */

static void RenderBreadcrumb(explorer_state *state, ui_context *ui,
                             rect bounds) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;

  /* Draw background */
  Render_DrawRect(ctx, bounds, th->panel);

  /* Draw path */
  i32 padding = 8;
  v2i pos = {bounds.x + padding, bounds.y + (bounds.h - 16) / 2};

  /* Truncate path if too long */
  const char *path = state->fs.current_path;
  i32 max_width = bounds.w - padding * 2;
  i32 path_width = Font_MeasureWidth(ui->font, path);

  if (path_width > max_width) {
    /* Show "..." prefix */
    char display[FS_MAX_PATH];
    const char *start = path + strlen(path);

    while (start > path) {
      start--;
      if (*start == '/') {
        char temp[FS_MAX_PATH];
        snprintf(temp, sizeof(temp), "...%s", start);
        if (Font_MeasureWidth(ui->font, temp) <= max_width) {
          snprintf(display, sizeof(display), "...%s", start);
          Render_DrawText(ctx, pos, display, ui->font, th->text);
          goto done_breadcrumb;
        }
      }
    }
    /* Fallback - just show what fits */
    Render_DrawText(ctx, pos, "...", ui->font, th->text);
  } else {
    Render_DrawText(ctx, pos, path, ui->font, th->text);
  }

done_breadcrumb:;

  /* Bottom border */
  {
    rect border = {bounds.x, bounds.y + bounds.h - 1, bounds.w, 1};
    Render_DrawRect(ctx, border, th->border);
  }
}

static void RenderFileItem(explorer_state *state, ui_context *ui,
                           fs_entry *entry, i32 index, rect bounds,
                           b32 is_selected, b32 is_hovered) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;

  /* Background */
  if (is_selected) {
    Render_DrawRectRounded(ctx, bounds, 4.0f, th->accent);
  } else if (is_hovered) {
    Render_DrawRectRounded(ctx, bounds, 4.0f, th->panel);
  }

  i32 x = bounds.x + EXPLORER_ICON_PADDING;
  i32 y = bounds.y;

  /* Icon */
  color icon_color =
      is_selected ? th->background : Icon_GetTypeColor(entry->icon, th);
  rect icon_bounds = {x, y + (bounds.h - EXPLORER_ICON_SIZE) / 2,
                      EXPLORER_ICON_SIZE, EXPLORER_ICON_SIZE};
  Icon_Draw(ctx, icon_bounds, entry->icon, icon_color);

  x += EXPLORER_ICON_SIZE + EXPLORER_ICON_PADDING;

  /* Filename */
  color text_color = is_selected ? th->background : th->text;
  v2i text_pos = {x, y + (bounds.h - Font_GetLineHeight(ui->font)) / 2};

  /* Check if file is hidden */
  b32 is_hidden = entry->name[0] == '.' && strcmp(entry->name, "..") != 0;
  if (is_hidden && !is_selected) {
    text_color = th->text_muted;
  }

  Render_DrawText(ctx, text_pos, entry->name, ui->font, text_color);

  /* Size column */
  if (state->show_size_column && !entry->is_directory) {
    char size_str[32];
    FS_FormatSize(entry->size, size_str, sizeof(size_str));

    i32 size_width = Font_MeasureWidth(ui->font, size_str);
    v2i size_pos = {bounds.x + bounds.w - size_width - 8,
                    y + (bounds.h - Font_GetLineHeight(ui->font)) / 2};
    Render_DrawText(ctx, size_pos, size_str, ui->font,
                    is_selected ? th->background : th->text_muted);
  }

  (void)index;
}

static void RenderDialog(explorer_state *state, ui_context *ui, rect bounds) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;

  /* Dim background */
  color dim = {0, 0, 0, 128};
  Render_DrawRect(ctx, bounds, dim);

  /* Dialog box */
  i32 dialog_w = 300;
  i32 dialog_h = 100;
  rect dialog = {bounds.x + (bounds.w - dialog_w) / 2,
                 bounds.y + (bounds.h - dialog_h) / 2, dialog_w, dialog_h};

  UI_DrawPanel(dialog);

  /* Title */
  const char *title = "";
  switch (state->mode) {
  case EXPLORER_MODE_RENAME:
    title = "Rename";
    break;
  case EXPLORER_MODE_CREATE_FILE:
    title = "New File";
    break;
  case EXPLORER_MODE_CREATE_DIR:
    title = "New Folder";
    break;
  case EXPLORER_MODE_CONFIRM_DELETE:
    title = "Delete?";
    break;
  default:
    break;
  }

  v2i title_pos = {dialog.x + 16, dialog.y + 12};
  Render_DrawText(ctx, title_pos, title, ui->font, th->text);

  /* Input or confirmation */
  if (state->mode == EXPLORER_MODE_CONFIRM_DELETE) {
    fs_entry *entry = FS_GetSelectedEntry(&state->fs);
    if (entry) {
      char msg[256];
      char name_truncated[64];
      strncpy(name_truncated, entry->name, sizeof(name_truncated) - 1);
      name_truncated[sizeof(name_truncated) - 1] = '\0';
      snprintf(msg, sizeof(msg), "Delete \"%s\"?", name_truncated);
      v2i msg_pos = {dialog.x + 16, dialog.y + 40};
      Render_DrawText(ctx, msg_pos, msg, ui->font, th->text);

      v2i hint_pos = {dialog.x + 16, dialog.y + 70};
      Render_DrawText(ctx, hint_pos, "Enter to confirm, Esc to cancel",
                      ui->font, th->text_muted);
    }
  } else {
    /* Text input */
    rect input_rect = {dialog.x + 16, dialog.y + 40, dialog.w - 32, 32};
    UI_BeginLayout(UI_LAYOUT_VERTICAL, input_rect);
    UI_TextInput(state->input_buffer, sizeof(state->input_buffer), "Name...",
                 &state->input_state);
    UI_EndLayout();
  }
}

void Explorer_Render(explorer_state *state, ui_context *ui, rect bounds,
                     b32 has_focus) {
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
       we want the scrollbar/etc to still sit flush, but visually the border is on top/under?
       Actually, let's just draw the border frame. */
  }

  /* Breadcrumb at top */
  rect breadcrumb = {bounds.x, bounds.y, bounds.w, EXPLORER_BREADCRUMB_HEIGHT};
  RenderBreadcrumb(state, ui, breadcrumb);

  /* File list area */
  rect list_area = {bounds.x, bounds.y + EXPLORER_BREADCRUMB_HEIGHT, bounds.w,
                    bounds.h - EXPLORER_BREADCRUMB_HEIGHT};

  /* Store bounds for mouse input handling */
  state->list_bounds = list_area;

  /* Setup scroll container */
  state->scroll.view_size = (v2f){(f32)list_area.w, (f32)list_area.h};
  /* Count visible items first for proper calculations */
  i32 visible_item_count = Explorer_CountVisible(state);
  state->scroll.content_size = (v2f){
      (f32)list_area.w, (f32)(visible_item_count * state->item_height)};

  /* Ensure selection is visible - only when navigation triggered it */
  if (state->scroll_to_selection) {
    /* Calculate the visible index of the selected entry */
    i32 visible_sel_index = 0;
    for (i32 i = 0; i < state->fs.selected_index; i++) {
      if (Explorer_IsEntryVisible(state, i)) {
        visible_sel_index++;
      }
    }
    
    i32 sel_y = visible_sel_index * state->item_height;
    f32 view_top = state->scroll.offset.y;
    f32 view_bottom = view_top + state->scroll.view_size.y - state->item_height;
    f32 max_scroll = state->scroll.content_size.y - state->scroll.view_size.y;

    /* Only scroll if content exceeds viewport */
    if (max_scroll > 0) {
      if ((f32)sel_y < view_top) {
        state->scroll.target_offset.y = (f32)sel_y;
        SmoothValue_SetTarget(&state->scroll.scroll_v,
                              state->scroll.target_offset.y);
      } else if ((f32)sel_y > view_bottom) {
        state->scroll.target_offset.y =
            (f32)sel_y - state->scroll.view_size.y + state->item_height;
        SmoothValue_SetTarget(&state->scroll.scroll_v,
                              state->scroll.target_offset.y);
      }
    }
    state->scroll_to_selection = false;
  }

  /* Clamp scroll */
  f32 max_scroll = state->scroll.content_size.y - state->scroll.view_size.y;
  if (max_scroll < 0)
    max_scroll = 0;
  if (state->scroll.target_offset.y < 0)
    state->scroll.target_offset.y = 0;
  if (state->scroll.target_offset.y > max_scroll)
    state->scroll.target_offset.y = max_scroll;

  /* Set clip rect for list */
  Render_SetClipRect(ctx, list_area);

  /* Draw visible items - iterate through all entries but track visible position
   */
  i32 visible_index = 0;
  i32 start_visible = (i32)(state->scroll.offset.y / state->item_height);
  i32 end_visible = start_visible + list_area.h / state->item_height + 2;

  for (u32 i = 0; i < state->fs.entry_count; i++) {
    fs_entry *entry = FS_GetEntry(&state->fs, (i32)i);
    if (!entry)
      continue;

    /* Skip hidden files if not showing them */
    if (!Explorer_IsEntryVisible(state, (i32)i)) {
      continue;
    }

    /* Only render if in visible range */
    if (visible_index >= start_visible && visible_index <= end_visible) {
      i32 item_y = list_area.y + (visible_index * state->item_height) -
                   (i32)state->scroll.offset.y;
      rect item_bounds = {list_area.x, item_y, list_area.w, state->item_height};

      b32 is_selected = ((i32)i == state->fs.selected_index);
      b32 is_hovered = UI_PointInRect(ui->input.mouse_pos, item_bounds);
      RenderFileItem(state, ui, entry, (i32)i, item_bounds, is_selected,
                     is_hovered);
    }

    visible_index++;
  }

  /* Reset clip */
  Render_ResetClipRect(ctx);

  /* Draw scrollbar if needed */
  if (state->scroll.content_size.y > state->scroll.view_size.y) {
    f32 ratio = state->scroll.view_size.y / state->scroll.content_size.y;
    i32 bar_height = (i32)(list_area.h * ratio);
    if (bar_height < 20)
      bar_height = 20;

    f32 scroll_ratio = max_scroll > 0 ? state->scroll.offset.y / max_scroll : 0;
    i32 bar_y = list_area.y + (i32)((list_area.h - bar_height) * scroll_ratio);

    rect scrollbar = {list_area.x + list_area.w - 8, bar_y, 6, bar_height};
    color bar_color = th->text_muted;
    bar_color.a = 100;
    Render_DrawRectRounded(ctx, scrollbar, 3.0f, bar_color);
  }

  /* Draw dialog overlay if in a mode */
  if (state->mode != EXPLORER_MODE_NORMAL) {
    RenderDialog(state, ui, bounds);
  }
}
