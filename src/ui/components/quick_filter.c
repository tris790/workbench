/*
 * quick_filter.c - Quick Filter Component Implementation
 *
 * C99, handmade hero style.
 */

#include "quick_filter.h"
#include "../../core/input.h"
#include "../../core/text.h"
#include "../../core/theme.h"
#include <string.h>

/* ===== Configuration ===== */

#define FILTER_PADDING 8
#define FILTER_MARGIN 8
#define FILTER_ICON_WIDTH 20

/* ===== Initialization ===== */

void QuickFilter_Init(quick_filter_state *state) {
  memset(state, 0, sizeof(*state));

  /* Initialize animation */
  state->fade_anim.speed = 600.0f; /* Fast fade for responsive feel */
  state->fade_anim.current = 0.0f;
  state->fade_anim.target = 0.0f;

  /* Initialize text input state */
  state->input_state.selection_start = -1;
  state->input_state.cursor_pos = 0;
}

/* ===== Update ===== */

b32 QuickFilter_Update(quick_filter_state *state, ui_context *ui) {
  /* Update fade animation */
  SmoothValue_Update(&state->fade_anim, ui->dt);

  /* Check for text input to activate filter if not already active */
  u32 text_input = Input_GetTextInput();
  if (!state->active && text_input >= 32 && text_input < 127) {
    state->active = true;
    state->fade_anim.target = 1.0f;
    state->input_state.cursor_pos = 0;
    state->input_state.selection_start = -1;
    state->buffer[0] = '\0';
  }

  if (!state->active) {
    return false;
  }

  /* Handle ESC to clear filter (Success Criteria 33) */
  if (ui->input.key_pressed[KEY_ESCAPE]) {
    QuickFilter_Clear(state);
    Input_ConsumeKeys();
    return true;
  }

  /* Process text input using shared engine (Requirement 12) */
  if (UI_ProcessTextInput(&state->input_state, state->buffer,
                          QUICK_FILTER_MAX_INPUT, &ui->input)) {
    /* Deactivate when buffer becomes empty (Requirement 22, Success Criteria
     * 32) */
    if (state->buffer[0] == '\0') {
      QuickFilter_Clear(state);
    }
    Input_ConsumeKeys();
    Input_ConsumeText();
    return true;
  }

  /* Still check if empty after any interaction (e.g. word delete) */
  if (state->buffer[0] == '\0') {
    QuickFilter_Clear(state);
  }

  /* Update cursor blink */
  state->input_state.cursor_blink += ui->dt * 2.0f;
  if (state->input_state.cursor_blink > 2.0f) {
    state->input_state.cursor_blink -= 2.0f;
  }

  /* Consume keys if we are active to prevent explorer navigation etc. */
  if (Input_GetTextInput() >= 32 || Input_KeyRepeat(KEY_BACKSPACE) ||
      Input_KeyRepeat(KEY_DELETE) || Input_KeyRepeat(KEY_LEFT) ||
      Input_KeyRepeat(KEY_RIGHT) || Input_KeyRepeat(KEY_HOME) ||
      Input_KeyRepeat(KEY_END) || (Input_GetModifiers() & MOD_CTRL)) {
    Input_ConsumeKeys();
    Input_ConsumeText();
    return true;
  }

  return true;
}

/* ===== Rendering ===== */

void QuickFilter_Render(quick_filter_state *state, ui_context *ui,
                        rect bounds) {
  /* Don't render if completely hidden */
  if (state->fade_anim.current < 0.01f) {
    return;
  }

  render_context *renderer = ui->renderer;
  const theme *th = ui->theme;
  font *f = ui->main_font;
  f32 fade = state->fade_anim.current;

  /* Calculate filter bar position (bottom of bounds, with margin) */
  i32 filter_width = bounds.w - (FILTER_MARGIN * 2);
  i32 filter_height = QUICK_FILTER_HEIGHT;
  i32 filter_x = bounds.x + FILTER_MARGIN;
  i32 filter_y = bounds.y + bounds.h - filter_height - FILTER_MARGIN;

  /* Apply fade animation to Y position (slide up from bottom) */
  filter_y += (i32)((1.0f - fade) * 10);

  rect filter_rect = {filter_x, filter_y, filter_width, filter_height};
  state->bounds = filter_rect;

  /* Background - semi-transparent panel with border */
  color bg = th->panel;
  bg.a = (u8)(230 * fade);

  color border = th->accent;
  border.a = (u8)(200 * fade);

  /* Draw border first (slightly larger rect) */
  rect border_rect = {filter_x - 1, filter_y - 1, filter_width + 2,
                      filter_height + 2};
  Render_DrawRectRounded(renderer, border_rect, th->radius_md + 1, border);

  /* Draw background */
  Render_DrawRectRounded(renderer, filter_rect, th->radius_md, bg);

  /* Search icon (magnifying glass character) */

  /* Filter text */
  i32 text_x = filter_x + FILTER_PADDING + FILTER_ICON_WIDTH;
  i32 text_y = filter_y + (filter_height - Font_GetLineHeight(f)) / 2;
  v2i text_pos = {text_x, text_y};

  color text_color = th->text;
  text_color.a = (u8)(text_color.a * fade);

  if (state->buffer[0] != '\0') {
    /* Draw selection highlight if any */
    if (state->input_state.selection_start >= 0) {
      i32 start =
          state->input_state.selection_start < state->input_state.selection_end
              ? state->input_state.selection_start
              : state->input_state.selection_end;
      i32 end =
          state->input_state.selection_start > state->input_state.selection_end
              ? state->input_state.selection_start
              : state->input_state.selection_end;

      char temp[QUICK_FILTER_MAX_INPUT];
      i32 start_byte = Text_UTF8ByteOffset(state->buffer, start);
      strncpy(temp, state->buffer, (size_t)start_byte);
      temp[start_byte] = '\0';
      i32 start_x = text_pos.x + Font_MeasureWidth(f, temp);

      i32 end_byte = Text_UTF8ByteOffset(state->buffer, end);
      strncpy(temp, state->buffer, (size_t)end_byte);
      temp[end_byte] = '\0';
      i32 end_x = text_pos.x + Font_MeasureWidth(f, temp);

      rect sel_rect = {start_x, text_y, end_x - start_x, Font_GetLineHeight(f)};
      color sel_bg = th->accent;
      sel_bg.a = (u8)(100 * fade);
      Render_DrawRect(renderer, sel_rect, sel_bg);
    }

    Render_DrawText(renderer, text_pos, state->buffer, f, text_color);

    /* Draw cursor at correct position */
    if (state->input_state.cursor_blink < 1.0f) {
      char temp[QUICK_FILTER_MAX_INPUT];
      i32 cursor_pos = state->input_state.cursor_pos;
      i32 char_count = Text_UTF8Length(state->buffer);
      if (cursor_pos > char_count)
        cursor_pos = char_count;

      i32 cursor_byte = Text_UTF8ByteOffset(state->buffer, cursor_pos);
      strncpy(temp, state->buffer, (size_t)cursor_byte);
      temp[cursor_byte] = '\0';

      i32 cursor_x = text_pos.x + Font_MeasureWidth(f, temp);
      rect cursor_rect = {cursor_x, text_y, 2, Font_GetLineHeight(f)};
      color cursor_color = th->accent;
      cursor_color.a = (u8)(cursor_color.a * fade);
      Render_DrawRect(renderer, cursor_rect, cursor_color);
    }
  } else {
    /* Placeholder text */
    color placeholder = th->text_muted;
    placeholder.a = (u8)(placeholder.a * fade);
    Render_DrawText(renderer, text_pos, "Type to filter...", f, placeholder);
  }

  /* ESC hint on the right */
  const char *hint = "ESC";
  i32 hint_width = Font_MeasureWidth(f, hint);
  v2i hint_pos = {filter_x + filter_width - hint_width - FILTER_PADDING,
                  text_y};

  color hint_color = th->text_muted;
  hint_color.a = (u8)(80 * fade);

  /* Draw hint background pill */
  rect hint_bg = {hint_pos.x - 4, filter_y + 6, hint_width + 8,
                  filter_height - 12};
  color hint_bg_color = th->panel_alt;
  hint_bg_color.a = (u8)(150 * fade);
  Render_DrawRectRounded(renderer, hint_bg, 4.0f, hint_bg_color);

  Render_DrawText(renderer, hint_pos, hint, f, hint_color);
}

/* ===== Utility ===== */

/* Clear the filter and hide the UI */
void QuickFilter_Clear(quick_filter_state *state) {
  state->buffer[0] = '\0';
  state->input_state.cursor_pos = 0;
  state->input_state.selection_start = -1;
  state->active = false;
  state->fade_anim.target = 0.0f;
}

void QuickFilter_ClearBuffer(quick_filter_state *state) {
  state->buffer[0] = '\0';
  state->input_state.cursor_pos = 0;
  state->input_state.selection_start = -1;
  /* Force active/visible */
  state->active = true;
  state->fade_anim.target = 1.0f;
}

void QuickFilter_SetBuffer(quick_filter_state *state, const char *text) {
  strncpy(state->buffer, text, QUICK_FILTER_MAX_INPUT - 1);
  state->buffer[QUICK_FILTER_MAX_INPUT - 1] = '\0';
  state->input_state.cursor_pos = (i32)strlen(state->buffer);
  state->input_state.selection_start = -1;
  /* Force active/visible */
  state->active = true;
  state->fade_anim.target = 1.0f;
}

b32 QuickFilter_IsActive(quick_filter_state *state) { return state->active; }

const char *QuickFilter_GetQuery(quick_filter_state *state) {
  return state->buffer;
}

void QuickFilter_Focus(quick_filter_state *state) {
  state->active = true;
  state->fade_anim.target = 1.0f;
  if (!Input_HasFocus(INPUT_TARGET_EXPLORER)) {
    /* If explorer isn't focused, we should probably focus it so filter gets
     * input */
    Input_PushFocus(INPUT_TARGET_EXPLORER);
  }
}
