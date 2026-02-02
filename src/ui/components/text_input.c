/*
 * text_input.c - UI Text Input Widget and Helpers
 */

#include "text_input.h"
#include "../../core/input.h"
#include "../../core/text.h"
#include "../ui.h"
#include <string.h>

/* Helper: push undo state */
void UI_TextInputPushUndo(ui_text_state *state, const char *text) {
  if (state->undo_count < UI_MAX_UNDO_STATES) {
    i32 idx = state->undo_index;
    strncpy(state->undo_stack[idx].text, text, UI_MAX_TEXT_INPUT_SIZE - 1);
    state->undo_stack[idx].text[UI_MAX_TEXT_INPUT_SIZE - 1] = '\0';
    state->undo_stack[idx].cursor_pos = state->cursor_pos;
    state->undo_index = (idx + 1) % UI_MAX_UNDO_STATES;
    state->undo_count++;
  }
}

/* Helper: pop undo state */
b32 UI_TextInputPopUndo(ui_text_state *state, char *buffer, i32 buf_size) {
  if (state->undo_count > 0) {
    state->undo_count--;
    state->undo_index =
        (state->undo_index - 1 + UI_MAX_UNDO_STATES) % UI_MAX_UNDO_STATES;
    strncpy(buffer, state->undo_stack[state->undo_index].text,
            (size_t)buf_size - 1);
    buffer[buf_size - 1] = '\0';
    state->cursor_pos = state->undo_stack[state->undo_index].cursor_pos;
    return true;
  }
  return false;
}

void UI_GetSelectionRange(ui_text_state *state, i32 *out_start, i32 *out_end) {
  if (state->selection_start <= state->selection_end) {
    *out_start = state->selection_start;
    *out_end = state->selection_end;
  } else {
    *out_start = state->selection_end;
    *out_end = state->selection_start;
  }
}

void UI_DeleteSelection(ui_text_state *state, char *buffer, i32 *text_len) {
  i32 start, end;
  UI_GetSelectionRange(state, &start, &end);
  i32 start_byte = Text_UTF8ByteOffset(buffer, start);
  i32 end_byte = Text_UTF8ByteOffset(buffer, end);
  UI_TextInputPushUndo(state, buffer);

  i32 total_bytes = (i32)strlen(buffer);
  memmove(buffer + start_byte, buffer + end_byte,
          (size_t)(total_bytes - end_byte + 1));
  state->cursor_pos = start;
  state->selection_start = -1;
  if (text_len) {
    *text_len = (i32)strlen(buffer);
  }
}

/* Process text input logic (shared by widgets) */
b32 UI_ProcessTextInput(ui_text_state *state, char *buffer, i32 buffer_size,
                        ui_input *input) {
  b32 changed = false;
  i32 text_len =
      (i32)strlen(buffer); /* Assumes UTF-8 well-formedness for length */

  /* Handle cursor blink reset on input */
  if (input->text_input || input->key_pressed[KEY_LEFT] ||
      input->key_pressed[KEY_RIGHT] || input->key_pressed[KEY_BACKSPACE] ||
      input->key_pressed[KEY_DELETE] || input->key_pressed[KEY_HOME] ||
      input->key_pressed[KEY_END]) {
    state->cursor_blink = 0.0f;
  }

  b32 ctrl = (input->modifiers & MOD_CTRL) != 0;
  b32 shift = (input->modifiers & MOD_SHIFT) != 0;

  /* Ctrl+A - Select all */
  if (ctrl && input->key_pressed[KEY_A]) {
    state->selection_start = 0;
    state->selection_end = Text_UTF8Length(buffer); /* CHARS */
    state->cursor_pos = state->selection_end;
  }

  /* Ctrl+C - Copy */
  if (ctrl && input->key_pressed[KEY_C]) {
    if (state->selection_start >= 0) {
      i32 start, end;
      UI_GetSelectionRange(state, &start, &end);
      i32 start_byte = Text_UTF8ByteOffset(buffer, start);
      i32 end_byte = Text_UTF8ByteOffset(buffer, end);
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      i32 len = end_byte - start_byte;
      if (len >= UI_MAX_TEXT_INPUT_SIZE)
        len = UI_MAX_TEXT_INPUT_SIZE - 1;

      strncpy(temp, buffer + start_byte, (size_t)len);
      temp[len] = '\0';
      Platform_SetClipboard(temp);
    }
  }

  /* Ctrl+X - Cut */
  if (ctrl && input->key_pressed[KEY_X]) {
    if (state->selection_start >= 0) {
      i32 start, end;
      UI_GetSelectionRange(state, &start, &end);
      i32 start_byte = Text_UTF8ByteOffset(buffer, start);
      i32 end_byte = Text_UTF8ByteOffset(buffer, end);

      /* Copy to clipboard */
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      i32 len = end_byte - start_byte;
      if (len >= UI_MAX_TEXT_INPUT_SIZE)
        len = UI_MAX_TEXT_INPUT_SIZE - 1;

      strncpy(temp, buffer + start_byte, (size_t)len);
      temp[len] = '\0';
      Platform_SetClipboard(temp);

      /* Delete selection */
      UI_DeleteSelection(state, buffer, &text_len);
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Ctrl+V - Paste */
  if (ctrl && input->key_pressed[KEY_V]) {
    char clipboard[UI_MAX_TEXT_INPUT_SIZE];
    if (Platform_GetClipboard(clipboard, sizeof(clipboard))) {
      /* Delete selection first if any */
      if (state->selection_start >= 0) {
        UI_DeleteSelection(state, buffer, &text_len);
      }

      /* Insert */
      i32 paste_len = (i32)strlen(clipboard);
      if (text_len + paste_len < buffer_size - 1) {
        UI_TextInputPushUndo(state, buffer);
        i32 cursor_byte = Text_UTF8ByteOffset(buffer, state->cursor_pos);
        i32 total_bytes = (i32)strlen(buffer);

        memmove(buffer + cursor_byte + paste_len, buffer + cursor_byte,
                (size_t)(total_bytes - cursor_byte + 1));
        memcpy(buffer + cursor_byte, clipboard, (size_t)paste_len);
        state->cursor_pos +=
            Text_UTF8Length(clipboard); /* Advance char count */
        changed = true;
        text_len = (i32)strlen(buffer);
      }
    }
  }

  /* Ctrl+Z - Undo */
  if (ctrl && input->key_pressed[KEY_Z]) {
    if (UI_TextInputPopUndo(state, buffer, buffer_size)) {
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Arrow keys for cursor movement */
  if (input->key_pressed[KEY_LEFT] || Input_KeyRepeat(KEY_LEFT)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }

    if (ctrl) {
      /* Word jump left */
      state->cursor_pos = Text_FindWordBoundaryLeft(buffer, state->cursor_pos);
    } else {
      if (state->cursor_pos > 0) {
        state->cursor_pos--;
      }
    }

    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  if (input->key_pressed[KEY_RIGHT] || Input_KeyRepeat(KEY_RIGHT)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }

    if (ctrl) {
      /* Word jump right */
      state->cursor_pos = Text_FindWordBoundaryRight(buffer, state->cursor_pos);
    } else {
      if (state->cursor_pos < Text_UTF8Length(buffer)) {
        state->cursor_pos++;
      }
    }

    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  /* Home/End */
  if (input->key_pressed[KEY_HOME] || Input_KeyRepeat(KEY_HOME)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }
    state->cursor_pos = 0;
    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  if (input->key_pressed[KEY_END] || Input_KeyRepeat(KEY_END)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }
    state->cursor_pos = Text_UTF8Length(buffer);
    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  /* Backspace - check both local key_pressed and global key repeat */
  if (input->key_pressed[KEY_BACKSPACE] || Input_KeyRepeat(KEY_BACKSPACE)) {
    if (state->selection_start >= 0) {
      /* Delete selection */
      UI_DeleteSelection(state, buffer, &text_len);
      changed = true;
    } else if (state->cursor_pos > 0) {
      UI_TextInputPushUndo(state, buffer);

      i32 target_pos = state->cursor_pos - 1;

      if (ctrl) {
        /* Word delete backward */
        target_pos = Text_FindWordBoundaryLeft(buffer, state->cursor_pos);
      }

      i32 byte_pos = Text_UTF8ByteOffset(buffer, state->cursor_pos);
      i32 target_byte = Text_UTF8ByteOffset(buffer, target_pos);
      i32 total_bytes = (i32)strlen(buffer);

      memmove(buffer + target_byte, buffer + byte_pos,
              (size_t)(total_bytes - byte_pos + 1));
      state->cursor_pos = target_pos;
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Delete - check both local key_pressed and global key repeat */
  if (input->key_pressed[KEY_DELETE] || Input_KeyRepeat(KEY_DELETE)) {
    if (state->selection_start >= 0) {
      /* Same as backspace with selection */
      UI_DeleteSelection(state, buffer, &text_len);
      changed = true;
    } else if (state->cursor_pos < Text_UTF8Length(buffer)) {
      UI_TextInputPushUndo(state, buffer);

      i32 target_pos = state->cursor_pos + 1;

      if (ctrl) {
        /* Word delete forward */
        target_pos = Text_FindWordBoundaryRight(buffer, state->cursor_pos);
      }

      i32 byte_pos = Text_UTF8ByteOffset(buffer, state->cursor_pos);
      i32 target_byte = Text_UTF8ByteOffset(buffer, target_pos);
      i32 total_bytes = (i32)strlen(buffer);

      memmove(buffer + byte_pos, buffer + target_byte,
              (size_t)(total_bytes - target_byte + 1));
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Text input */
  if (input->text_input && input->text_input >= 32) {
    /* Delete selection first if any */
    if (state->selection_start >= 0) {
      UI_DeleteSelection(state, buffer, &text_len);
    }

    /* Insert character */
    if (text_len < buffer_size - 2) {
      /* Simple ASCII for now */
      if (input->text_input < 128) {
        i32 byte_pos = Text_UTF8ByteOffset(buffer, state->cursor_pos);
        /* Use fresh length */
        i32 total_bytes = (i32)strlen(buffer);
        memmove(buffer + byte_pos + 1, buffer + byte_pos,
                (size_t)(total_bytes - byte_pos + 1));
        buffer[byte_pos] = (char)input->text_input;
        state->cursor_pos++;
        changed = true;
      }
    }
  }

  return changed;
}

b32 UI_TextInput(char *buffer, i32 buffer_size, const char *placeholder,
                 ui_text_state *state) {
  ui_id id = UI_GenID(placeholder ? placeholder : "##textinput");

  UI_RegisterFocusable(id);

  i32 padding = UI_GetStyleInt(UI_STYLE_PADDING);
  i32 font_height = Font_GetLineHeight(g_ui_ctx->font);
  i32 height = font_height + padding * 2;

  rect avail = UI_GetAvailableRect();
  rect bounds = {avail.x, avail.y, avail.w, height};

  /* Interaction */
  b32 hovered = UI_PointInRect(g_ui_ctx->input.mouse_pos, bounds);
  b32 changed = false;

  if (hovered && g_ui_ctx->input.mouse_pressed[MOUSE_LEFT]) {
    g_ui_ctx->focused = id;
    state->has_focus = true;

    /* Calculate cursor position from click */
    i32 click_x = g_ui_ctx->input.mouse_pos.x - bounds.x - padding;
    i32 text_len = Text_UTF8Length(buffer);
    i32 best_pos = 0;
    i32 best_dist = 10000;

    for (i32 i = 0; i <= text_len; i++) {
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      strncpy(temp, buffer, (size_t)Text_UTF8ByteOffset(buffer, i));
      temp[Text_UTF8ByteOffset(buffer, i)] = '\0';
      i32 w = Font_MeasureWidth(g_ui_ctx->font, temp);
      i32 dist = click_x > w ? click_x - w : w - click_x;
      if (dist < best_dist) {
        best_dist = dist;
        best_pos = i;
      }
    }
    state->cursor_pos = best_pos;
    state->selection_start = -1;
  }

  /* Focus management */
  if (g_ui_ctx->focused == id) {
    state->has_focus = true;
  } else if (state->has_focus) {
    /* Check if we just lost focus or if we are claiming it */
    if (g_ui_ctx->last_focused != id) {
      /* We weren't focused last frame, but state says we should be.
         Claim focus. */
      g_ui_ctx->focused = id;
    } else {
      /* We were focused and lost it. */
      state->has_focus = false;
      state->selection_start = -1;
    }
  }

  /* Handle keyboard input when focused */
  if (g_ui_ctx->focused == id) {
    if (UI_ProcessTextInput(state, buffer, buffer_size, &g_ui_ctx->input)) {
      changed = true;
    }

    /* Update cursor blink */
    if (!g_animations_enabled) {
      state->cursor_blink = 0.0f; /* Always show cursor */
    } else {
      state->cursor_blink += g_ui_ctx->dt * 2.0f;
      if (state->cursor_blink > 2.0f)
        state->cursor_blink -= 2.0f;
    }
  }

  /* Drawing */
  color bg = g_ui_ctx->theme->panel_alt;
  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  color border_color = UI_GetStyleColor(UI_STYLE_BORDER_COLOR);
  f32 radius = UI_GetStyleFloat(UI_STYLE_BORDER_RADIUS);

  /* Focus highlight */
  if (g_ui_ctx->focused == id) {
    border_color = UI_GetStyleColor(UI_STYLE_ACCENT_COLOR);
  }

  /* Draw border first */
  Render_DrawRectRounded(g_ui_ctx->renderer, bounds, radius, border_color);

  /* Background (inset by 1px for border) */
  rect inner = {bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2};
  Render_DrawRectRounded(g_ui_ctx->renderer, inner, radius - 1, bg);

  /* Set clip for text */
  rect text_clip = {bounds.x + padding, bounds.y, bounds.w - padding * 2,
                    bounds.h};
  Render_SetClipRect(g_ui_ctx->renderer, text_clip);

  v2i text_pos = {bounds.x + padding, bounds.y + padding};
  b32 show_placeholder = (buffer[0] == '\0');

  if (show_placeholder && placeholder) {
    color placeholder_color = g_ui_ctx->theme->text_muted;
    Render_DrawText(g_ui_ctx->renderer, text_pos, placeholder, g_ui_ctx->font,
                    placeholder_color);
  } else {
    /* Draw selection highlight */
    if (state->selection_start >= 0 && g_ui_ctx->focused == id) {
      i32 start, end;
      UI_GetSelectionRange(state, &start, &end);

      char temp[UI_MAX_TEXT_INPUT_SIZE];
      strncpy(temp, buffer, (size_t)Text_UTF8ByteOffset(buffer, start));
      temp[Text_UTF8ByteOffset(buffer, start)] = '\0';
      i32 start_x = text_pos.x + Font_MeasureWidth(g_ui_ctx->font, temp);

      strncpy(temp, buffer, (size_t)Text_UTF8ByteOffset(buffer, end));
      temp[Text_UTF8ByteOffset(buffer, end)] = '\0';
      i32 end_x = text_pos.x + Font_MeasureWidth(g_ui_ctx->font, temp);

      rect sel_rect = {start_x, bounds.y + 2, end_x - start_x, bounds.h - 4};
      color sel_color = UI_GetStyleColor(UI_STYLE_ACCENT_COLOR);
      sel_color.a = 128;
      Render_DrawRect(g_ui_ctx->renderer, sel_rect, sel_color);
    }

    /* Draw text */
    Render_DrawText(g_ui_ctx->renderer, text_pos, buffer, g_ui_ctx->font,
                    text_color);

    /* Draw cursor */
    if (g_ui_ctx->focused == id && state->cursor_blink < 1.0f) {
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      strncpy(temp, buffer,
              (size_t)Text_UTF8ByteOffset(buffer, state->cursor_pos));
      temp[Text_UTF8ByteOffset(buffer, state->cursor_pos)] = '\0';
      i32 cursor_x = text_pos.x + Font_MeasureWidth(g_ui_ctx->font, temp);

      rect cursor_rect = {cursor_x, bounds.y + 3, 2, bounds.h - 6};
      Render_DrawRect(g_ui_ctx->renderer, cursor_rect, text_color);
    }
  }

  /* Restore clip */
  Render_ResetClipRect(g_ui_ctx->renderer);

  UI_AdvanceLayout(bounds.w, height);

  return changed;
}
