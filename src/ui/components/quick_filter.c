/*
 * quick_filter.c - Quick Filter Component Implementation
 *
 * C99, handmade hero style.
 */

#include "quick_filter.h"
#include "input.h"
#include "theme.h"
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

  /* Update cursor blink when active */
  if (state->active) {
    state->input_state.cursor_blink += ui->dt * 2.0f;
    if (state->input_state.cursor_blink > 2.0f) {
      state->input_state.cursor_blink -= 2.0f;
    }
  }

  /* Check for ESC to clear filter */
  if (state->active && ui->input.key_pressed[KEY_ESCAPE]) {
    QuickFilter_Clear(state);
    return true; /* Consumed the escape key */
  }

  /* Check for text input to activate/update filter */
  u32 text_input = Input_GetTextInput();

  /* Accept printable ASCII characters */
  if (text_input >= 32 && text_input < 127) {
    /* Append character to buffer */
    i32 len = (i32)strlen(state->buffer);
    if (len < QUICK_FILTER_MAX_INPUT - 1) {
      state->buffer[len] = (char)text_input;
      state->buffer[len + 1] = '\0';
      state->input_state.cursor_pos = len + 1;

      /* Activate filter if not already active */
      if (!state->active) {
        state->active = true;
        state->fade_anim.target = 1.0f;
      }

      /* Consume the text input */
      Input_ConsumeText();
      Input_ConsumeKeys();
      return true;
    }
  }

  /* Handle backspace when filter is active */
  if (state->active && Input_KeyRepeat(KEY_BACKSPACE)) {
    i32 len = (i32)strlen(state->buffer);
    if (len > 0) {
      state->buffer[len - 1] = '\0';
      state->input_state.cursor_pos = len - 1;

      /* If buffer is now empty, deactivate filter */
      if (state->buffer[0] == '\0') {
        QuickFilter_Clear(state);
      }

      Input_ConsumeKeys();
      return true;
    }
  }

  return state->active;
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
  font *f = ui->font;
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
    Render_DrawText(renderer, text_pos, state->buffer, f, text_color);

    /* Draw cursor */
    if (state->input_state.cursor_blink < 1.0f) {
      i32 cursor_x = text_x + Font_MeasureWidth(f, state->buffer);
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
