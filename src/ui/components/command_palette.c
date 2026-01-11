/*
 * command_palette.c - Command Palette Implementation
 *
 * VSCode-style command palette with file search and command mode.
 * C99, handmade hero style.
 */

#include "command_palette.h"
#include "fuzzy_match.h"
#include "input.h"
#include <stdio.h>
#include <string.h>

/* ===== Internal Helpers ===== */

/* Populate items list for file mode */
static void PopulateFileItems(command_palette_state *state) {
  state->item_count = 0;
  if (!state->fs)
    return;

  const char *query = state->input_buffer;

  for (u32 i = 0;
       i < state->fs->entry_count && state->item_count < PALETTE_MAX_ITEMS;
       i++) {
    fs_entry *entry = &state->fs->entries[i];

    /* Filter by query */
    if (query[0] != '\0' && !FuzzyMatch(query, entry->name)) {
      continue;
    }

    palette_item *item = &state->items[state->item_count];
    memset(item, 0, sizeof(*item));
    snprintf(item->label, sizeof(item->label), "%s", entry->name);
    item->icon = entry->icon;
    item->is_file = true;
    item->user_data = entry;

    state->item_count++;
  }
}

/* Populate items list for command mode */
static void PopulateCommandItems(command_palette_state *state) {
  state->item_count = 0;

  /* Skip the '>' prefix if present */
  const char *query = state->input_buffer;
  if (query[0] == '>')
    query++;
  while (*query == ' ')
    query++;

  for (i32 i = 0;
       i < state->command_count && state->item_count < PALETTE_MAX_ITEMS; i++) {
    palette_command *cmd = &state->commands[i];

    /* Filter by query */
    if (query[0] != '\0' && !FuzzyMatch(query, cmd->name)) {
      continue;
    }

    palette_item *item = &state->items[state->item_count];
    memset(item, 0, sizeof(*item));
    snprintf(item->label, sizeof(item->label), "%s", cmd->name);
    snprintf(item->shortcut, sizeof(item->shortcut), "%s", cmd->shortcut);
    snprintf(item->category, sizeof(item->category), "%s", cmd->category);
    item->icon = FILE_ICON_UNKNOWN; /* Commands don't have file icons */
    item->is_file = false;
    item->callback = cmd->callback;
    item->user_data = cmd->user_data;

    state->item_count++;
  }
}

/* Execute selected item */
static void ExecuteSelectedItem(command_palette_state *state) {
  if (state->selected_index < 0 || state->selected_index >= state->item_count)
    return;

  palette_item *item = &state->items[state->selected_index];

  if (item->is_file) {
    /* Navigate to file or directory */
    fs_entry *entry = (fs_entry *)item->user_data;
    if (entry && entry->is_directory) {
      FS_LoadDirectory(state->fs, entry->path);
    }
  } else {
    /* Execute command callback */
    if (item->callback) {
      item->callback(item->user_data);
    }
  }

  CommandPalette_Close(state);
}

/* ===== Public API ===== */

void CommandPalette_Init(command_palette_state *state, fs_state *fs) {
  memset(state, 0, sizeof(*state));
  state->fs = fs;
  state->mode = PALETTE_MODE_CLOSED;
  state->item_height = 28;
  state->selected_index = 0;

  /* Initialize smooth animation */
  state->fade_anim.current = 0.0f;
  state->fade_anim.target = 0.0f;
  state->fade_anim.speed = 10.0f;

  /* Initialize scroll animation speed */
  state->scroll.scroll_v.speed = 1500.0f;
}

void CommandPalette_RegisterCommand(command_palette_state *state,
                                    const char *name, const char *shortcut,
                                    const char *category,
                                    command_callback callback,
                                    void *user_data) {
  if (state->command_count >= PALETTE_MAX_COMMANDS)
    return;

  palette_command *cmd = &state->commands[state->command_count];
  snprintf(cmd->name, sizeof(cmd->name), "%s", name);
  snprintf(cmd->shortcut, sizeof(cmd->shortcut), "%s",
           shortcut ? shortcut : "");
  snprintf(cmd->category, sizeof(cmd->category), "%s",
           category ? category : "");
  cmd->callback = callback;
  cmd->user_data = user_data;

  state->command_count++;
}

void CommandPalette_Open(command_palette_state *state, palette_mode mode) {
  state->mode = mode;
  state->selected_index = 0;
  state->just_opened = true;

  /* Clear input and set prefix for command mode */
  memset(state->input_buffer, 0, sizeof(state->input_buffer));
  memset(&state->input_state, 0, sizeof(state->input_state));
  state->input_state.selection_start = -1; /* -1 means no selection */

  if (mode == PALETTE_MODE_COMMAND) {
    state->input_buffer[0] = '>';
    state->input_buffer[1] = ' ';
    state->input_state.cursor_pos = 2;
  }

  /* Start fade-in animation */
  state->fade_anim.target = 1.0f;

  /* Push focus to command palette */
  Input_PushFocus(INPUT_TARGET_COMMAND_PALETTE);

  /* Reset scroll position (preserve speed) */
  state->scroll.offset = (v2f){0, 0};
  state->scroll.target_offset = (v2f){0, 0};
  state->scroll.scroll_v.current = 0;
  state->scroll.scroll_v.target = 0;

  /* Populate initial items */
  if (mode == PALETTE_MODE_FILE) {
    PopulateFileItems(state);
  } else {
    PopulateCommandItems(state);
  }
}

void CommandPalette_Close(command_palette_state *state) {
  state->mode = PALETTE_MODE_CLOSED;
  state->fade_anim.target = 0.0f;
  Input_PopFocus();
}

b32 CommandPalette_IsOpen(command_palette_state *state) {
  return state->mode != PALETTE_MODE_CLOSED;
}

b32 CommandPalette_Update(command_palette_state *state, ui_context *ui) {
  /* Always update fade animation (for fade-out when closing) */
  SmoothValue_Update(&state->fade_anim, ui->dt);

  if (!CommandPalette_IsOpen(state))
    return false;

  /* Register as active modal */
  UI_BeginModal("CommandPalette");

  ui_input *input = &ui->input;

  /* Handle escape to close */
  if (input->key_pressed[KEY_ESCAPE]) {
    CommandPalette_Close(state);
    UI_EndModal();
    return true;
  }

  /* Handle enter to execute */
  if (input->key_pressed[KEY_RETURN]) {
    ExecuteSelectedItem(state);
    return true;
  }

  /* Track modifier keys */
  b32 ctrl = (input->modifiers & MOD_CTRL) != 0;

  /* Handle up/down navigation (only when not holding ctrl for text editing) */
  if (Input_KeyRepeat(KEY_UP) && !ctrl) {
    if (state->selected_index > 0) {
      state->selected_index--;
      state->scroll_to_selection = true;
    }
    return true;
  }

  if (Input_KeyRepeat(KEY_DOWN) && !ctrl) {
    if (state->selected_index < state->item_count - 1) {
      state->selected_index++;
      state->scroll_to_selection = true;
    }
    return true;
  }

  /* ===== Text Input Handling (standard keybinds) ===== */

  /* Delegate to shared text input processing */
  /* Up/Down are NOT handled by ProcessTextInput so list nav still works */
  if (UI_ProcessTextInput(&state->input_state, state->input_buffer,
                          PALETTE_MAX_INPUT, input)) {
    if (state->mode == PALETTE_MODE_FILE) {
      PopulateFileItems(state);
    } else {
      PopulateCommandItems(state);
    }
    state->selected_index = 0;
    state->scroll_to_selection = true;
  }

  UI_EndModal();
  return true; /* Consume all input when palette is open */
}

void CommandPalette_Render(command_palette_state *state, ui_context *ui,
                           i32 win_width, i32 win_height) {
  if (!CommandPalette_IsOpen(state) && state->fade_anim.current < 0.01f)
    return;

  render_context *renderer = ui->renderer;
  const theme *th = ui->theme;
  font *f = ui->font;
  ui_input *input = &ui->input;

  f32 fade = state->fade_anim.current;

  /* ===== Backdrop ===== */
  color backdrop = {0, 0, 0, (u8)(128 * fade)};
  Render_DrawRect(renderer, (rect){0, 0, win_width, win_height}, backdrop);

  /* ===== Panel Dimensions (centered, VSCode style) ===== */
  i32 panel_width = 600;
  if (panel_width > win_width - 40)
    panel_width = win_width - 40;

  i32 input_height = 36;
  i32 max_visible_items = 10;
  i32 visible_items = state->item_count < max_visible_items ? state->item_count
                                                            : max_visible_items;
  i32 list_height = visible_items * state->item_height;
  i32 panel_height = input_height + list_height + 4; /* +4 for separator */

  i32 panel_x = (win_width - panel_width) / 2;
  i32 panel_y = win_height / 6; /* Position near top like VSCode */

  /* Apply fade animation to Y position */
  panel_y -= (i32)((1.0f - fade) * 20);

  state->panel_bounds = (rect){panel_x, panel_y, panel_width, panel_height};

  /* ===== Panel Background ===== */
  color panel_bg = th->panel;
  panel_bg.a = (u8)(panel_bg.a * fade);
  Render_DrawRectRounded(renderer, state->panel_bounds, th->radius_md,
                         panel_bg);

  /* ===== Panel Border ===== */
  color border_color = th->border;
  border_color.a = (u8)(border_color.a * fade);
  /* Draw border by rendering slightly larger rect behind (simple approach) */
  rect border_rect = {panel_x - 1, panel_y - 1, panel_width + 2,
                      panel_height + 2};
  Render_DrawRectRounded(renderer, border_rect, th->radius_md + 1,
                         border_color);
  Render_DrawRectRounded(renderer, state->panel_bounds, th->radius_md,
                         panel_bg);

  /* ===== Input Field ===== */
  rect input_rect = {panel_x + 8, panel_y + 4, panel_width - 16,
                     input_height - 4};

  /* Draw text input */
  UI_BeginLayout(UI_LAYOUT_HORIZONTAL, input_rect);

  /* Draw input prefix/icon based on mode */
  color text_color = th->text;
  text_color.a = (u8)(text_color.a * fade);

  i32 padding = 8;
  v2i text_pos = {input_rect.x + padding,
                  input_rect.y + (input_height - 4) / 2 - 8};

  /* Draw selection highlight if any */
  if (state->input_state.selection_start >= 0 &&
      state->input_buffer[0] != '\0') {
    i32 sel_start =
        state->input_state.selection_start < state->input_state.selection_end
            ? state->input_state.selection_start
            : state->input_state.selection_end;
    i32 sel_end =
        state->input_state.selection_start > state->input_state.selection_end
            ? state->input_state.selection_start
            : state->input_state.selection_end;

    /* Measure text up to selection start */
    char temp[PALETTE_MAX_INPUT];
    strncpy(temp, state->input_buffer, (size_t)sel_start);
    temp[sel_start] = '\0';
    i32 start_x = text_pos.x + Font_MeasureWidth(f, temp);

    /* Measure text up to selection end */
    strncpy(temp, state->input_buffer, (size_t)sel_end);
    temp[sel_end] = '\0';
    i32 end_x = text_pos.x + Font_MeasureWidth(f, temp);

    rect sel_rect = {start_x, text_pos.y, end_x - start_x, 18};
    color sel_bg = th->accent;
    sel_bg.a = (u8)(100 * fade);
    Render_DrawRect(renderer, sel_rect, sel_bg);
  }

  /* Render text input content */
  if (state->input_buffer[0] != '\0') {
    Render_DrawText(renderer, text_pos, state->input_buffer, f, text_color);
  } else {
    /* Placeholder */
    color placeholder = th->text_muted;
    placeholder.a = (u8)(placeholder.a * fade);
    const char *hint = state->mode == PALETTE_MODE_FILE ? "Search files..."
                                                        : "Type a command...";
    Render_DrawText(renderer, text_pos, hint, f, placeholder);
  }

  /* Blinking cursor at cursor position (don't blink when there's a selection)
   */
  if (state->just_opened || CommandPalette_IsOpen(state)) {
    state->input_state.cursor_blink += ui->dt;
    b32 show_cursor = (state->input_state.selection_start >= 0) ||
                      ((i32)(state->input_state.cursor_blink * 2) % 2 == 0);
    if (show_cursor) {
      /* Measure text up to cursor position */
      char temp[PALETTE_MAX_INPUT];
      i32 cursor_pos = state->input_state.cursor_pos;
      if (cursor_pos > (i32)strlen(state->input_buffer)) {
        cursor_pos = (i32)strlen(state->input_buffer);
      }
      strncpy(temp, state->input_buffer, (size_t)cursor_pos);
      temp[cursor_pos] = '\0';
      v2i text_size = UI_MeasureText(temp, f);
      i32 cursor_x = text_pos.x + text_size.x;
      Render_DrawRect(renderer, (rect){cursor_x, text_pos.y, 2, 18},
                      th->accent);
    }
  }

  UI_EndLayout();

  /* ===== Separator Line ===== */
  i32 sep_y = panel_y + input_height;
  Render_DrawRect(renderer, (rect){panel_x, sep_y, panel_width, 1},
                  border_color);

  /* ===== Item List ===== */
  rect list_rect = {panel_x, sep_y + 1, panel_width, list_height};

  /* Calculate total content height and max scroll */
  i32 total_content_height = state->item_count * state->item_height;
  f32 max_scroll = (f32)(total_content_height - list_height);
  if (max_scroll < 0)
    max_scroll = 0;

  /* Handle mouse wheel scrolling */
  if (UI_PointInRect(input->mouse_pos, list_rect) && input->scroll_delta != 0) {
    state->scroll.target_offset.y -= input->scroll_delta * 40.0f;
    /* Clamp to bounds */
    if (state->scroll.target_offset.y < 0) {
      state->scroll.target_offset.y = 0;
    }
    if (state->scroll.target_offset.y > max_scroll) {
      state->scroll.target_offset.y = max_scroll;
    }
    state->scroll.scroll_v.target = state->scroll.target_offset.y;
  }

  /* Handle scroll to selection */
  if (state->scroll_to_selection && state->item_count > 0) {
    i32 sel_top = state->selected_index * state->item_height;
    i32 sel_bottom = sel_top + state->item_height;
    i32 view_top = (i32)state->scroll.offset.y;
    i32 view_bottom = view_top + list_height;

    if (sel_top < view_top) {
      state->scroll.target_offset.y = (f32)sel_top;
    } else if (sel_bottom > view_bottom) {
      state->scroll.target_offset.y = (f32)(sel_bottom - list_height);
    }

    /* Clamp target after scroll-to-selection */
    if (state->scroll.target_offset.y < 0) {
      state->scroll.target_offset.y = 0;
    }
    if (state->scroll.target_offset.y > max_scroll) {
      state->scroll.target_offset.y = max_scroll;
    }
    state->scroll.scroll_v.target = state->scroll.target_offset.y;
    state->scroll_to_selection = false;
  }

  /* Smooth scroll animation */
  SmoothValue_Update(&state->scroll.scroll_v, ui->dt);
  state->scroll.offset.y = state->scroll.scroll_v.current;

  /* Clip to list area */
  Render_SetClipRect(renderer, list_rect);

  /* Render items */
  for (i32 i = 0; i < state->item_count; i++) {
    palette_item *item = &state->items[i];

    i32 item_y =
        list_rect.y + i * state->item_height - (i32)state->scroll.offset.y;

    /* Skip items outside visible area */
    if (item_y + state->item_height < list_rect.y ||
        item_y > list_rect.y + list_height)
      continue;

    rect item_rect = {list_rect.x, item_y, list_rect.w, state->item_height};

    /* Selection highlight */
    b32 selected = (i == state->selected_index);
    if (selected) {
      color sel_bg = th->selection;
      sel_bg.a = (u8)(sel_bg.a * fade);
      Render_DrawRect(renderer, item_rect, sel_bg);
    }

    /* Check for mouse hover */
    if (UI_PointInRect(ui->input.mouse_pos, item_rect)) {
      if (!selected) {
        color hover_bg = th->highlight;
        hover_bg.a = (u8)(40 * fade);
        Render_DrawRect(renderer, item_rect, hover_bg);
      }
      /* Click to select and execute */
      if (ui->input.mouse_pressed[MOUSE_LEFT]) {
        state->selected_index = i;
        ExecuteSelectedItem(state);
      }
    }

    /* Item text */
    i32 text_x = item_rect.x + 12;
    i32 text_y = item_rect.y + (state->item_height - 16) / 2;

    color item_text = selected ? th->text : th->text;
    item_text.a = (u8)(item_text.a * fade);

    Render_DrawText(renderer, (v2i){text_x, text_y}, item->label, f, item_text);

    /* Shortcut on right side (for commands) */
    if (item->shortcut[0] != '\0') {
      v2i shortcut_size = UI_MeasureText(item->shortcut, f);
      i32 shortcut_x = item_rect.x + item_rect.w - shortcut_size.x - 12;

      color shortcut_color = th->text_muted;
      shortcut_color.a = (u8)(shortcut_color.a * fade);
      Render_DrawText(renderer, (v2i){shortcut_x, text_y}, item->shortcut, f,
                      shortcut_color);
    }

    /* Category on right side (if no shortcut) */
    if (item->shortcut[0] == '\0' && item->category[0] != '\0') {
      v2i cat_size = UI_MeasureText(item->category, f);
      i32 cat_x = item_rect.x + item_rect.w - cat_size.x - 12;

      color cat_color = th->accent;
      cat_color.a = (u8)(cat_color.a * fade);
      Render_DrawText(renderer, (v2i){cat_x, text_y}, item->category, f,
                      cat_color);
    }
  }

  /* Clear clip */
  Render_ResetClipRect(renderer);

  /* Clear just_opened flag after first frame */
  state->just_opened = false;
}
