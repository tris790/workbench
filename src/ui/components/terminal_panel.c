/*
 * terminal_panel.c - Terminal panel UI component implementation
 *
 * Handles terminal rendering, input, and panel management.
 * C99, handmade hero style.
 */

#include "terminal_panel.h"
#include "../../core/input.h"
#include "../../core/theme.h"
#include "../../platform/platform.h"
#include "../../renderer/renderer.h"

#include <stdlib.h>
#include <string.h>

/* Terminal dimensions in cells */
#define TERMINAL_DEFAULT_COLS 80
#define TERMINAL_DEFAULT_ROWS 24
#define TERMINAL_DEFAULT_HEIGHT_RATIO 0.35f

/* 16-color ANSI palette (matches common terminal colors) */
static const color ansi_colors[16] = {
    {30, 30, 30, 255},    /* 0: Black */
    {205, 49, 49, 255},   /* 1: Red */
    {13, 188, 121, 255},  /* 2: Green */
    {229, 229, 16, 255},  /* 3: Yellow */
    {36, 114, 200, 255},  /* 4: Blue */
    {188, 63, 188, 255},  /* 5: Magenta */
    {17, 168, 205, 255},  /* 6: Cyan */
    {229, 229, 229, 255}, /* 7: White */
    {102, 102, 102, 255}, /* 8: Bright Black */
    {241, 76, 76, 255},   /* 9: Bright Red */
    {35, 209, 139, 255},  /* 10: Bright Green */
    {245, 245, 67, 255},  /* 11: Bright Yellow */
    {59, 142, 234, 255},  /* 12: Bright Blue */
    {214, 112, 214, 255}, /* 13: Bright Magenta */
    {41, 184, 219, 255},  /* 14: Bright Cyan */
    {255, 255, 255, 255}, /* 15: Bright White */
};

static color get_ansi_color(u8 idx, b32 is_fg, b32 bold) {
  if (idx < 16) {
    /* For bold foreground, use bright variant if in 0-7 range */
    if (is_fg && bold && idx < 8) {
      return ansi_colors[idx + 8];
    }
    return ansi_colors[idx];
  }

  /* 256-color mode: 16-231 are 6x6x6 color cube */
  if (idx >= 16 && idx < 232) {
    u8 n = idx - 16;
    u8 r = (n / 36) * 51;
    u8 g = ((n / 6) % 6) * 51;
    u8 b = (n % 6) * 51;
    return (color){r, g, b, 255};
  }

  /* 232-255 are grayscale */
  if (idx >= 232) {
    u8 gray = 8 + (idx - 232) * 10;
    return (color){gray, gray, gray, 255};
  }

  return ansi_colors[7]; /* Default white */
}

void TerminalPanel_Init(terminal_panel_state *state) {
  memset(state, 0, sizeof(terminal_panel_state));
  state->height_ratio = TERMINAL_DEFAULT_HEIGHT_RATIO;
  /* Manual smooth_value initialization */
  state->anim.current = 0.0f;
  state->anim.target = 0.0f;
  state->anim.speed = 8.0f;
  state->cursor_blink.current = 1.0f;
  state->cursor_blink.target = 1.0f;
  state->cursor_blink.speed = 4.0f;

  /* Initialize suggestion engine */
#ifdef _WIN32
  state->shell_mode =
      SHELL_EMULATED; /* Enable fish emulation on Windows by default */
#else
  state->shell_mode =
      SHELL_FISH; /* Default to fish shell on Linux per user request */
#endif
  state->suggestions = Suggestion_Create(NULL); /* Default history path */
  state->selection_scroll_accumulator = 0.0f;
}

void TerminalPanel_Destroy(terminal_panel_state *state) {
  if (!state)
    return;

  if (state->terminal) {
    Terminal_Destroy(state->terminal);
    state->terminal = NULL;
  }

  if (state->suggestions) {
    Suggestion_Destroy(state->suggestions);
    state->suggestions = NULL;
  }
}

void TerminalPanel_Toggle(terminal_panel_state *state, const char *cwd) {
  if (!state)
    return;

  state->visible = !state->visible;

  if (state->visible) {
    /* Opening terminal */
    SmoothValue_SetTarget(&state->anim, 1.0f);
    state->has_focus = true;
    Input_SetFocus(INPUT_TARGET_TERMINAL);

    /* Create terminal if needed, or respawn if CWD changed significantly */
    if (!state->terminal) {
      state->terminal =
          Terminal_Create(TERMINAL_DEFAULT_COLS, TERMINAL_DEFAULT_ROWS);
      if (state->terminal) {
        const char *shell = NULL;
        if (state->shell_mode == SHELL_FISH) {
          shell = "fish";
        }
        Terminal_Spawn(state->terminal, shell, cwd);
        if (cwd) {
          strncpy(state->last_cwd, cwd, sizeof(state->last_cwd) - 1);
        }
      }
    }
  } else {
    /* Closing terminal (keep session alive) */
    SmoothValue_SetTarget(&state->anim, 0.0f);
    state->has_focus = false;
    Input_SetFocus(INPUT_TARGET_EXPLORER);
  }
}

void TerminalPanel_Update(terminal_panel_state *state, ui_context *ui, f32 dt,
                          b32 is_active, f32 available_height) {
  (void)ui; /* Used to be for ui->input, now using centralized Input_* API */
  if (!state)
    return;

  /* Update animation */
  SmoothValue_Update(&state->anim, dt);

  /* Sync has_focus with global input focus AND panel activity */
  state->has_focus =
      Input_HasFocus(INPUT_TARGET_TERMINAL) && state->visible && is_active;

  /* Update cursor blink */
  if (state->visible && state->has_focus) {
    /* Toggle blink every ~500ms */
    f32 blink_target = (state->cursor_blink.current < 0.5f) ? 1.0f : 0.0f;
    static f32 blink_timer = 0.0f;
    blink_timer += dt;
    if (blink_timer > 0.5f) {
      SmoothValue_SetTarget(&state->cursor_blink, blink_target);
      blink_timer = 0.0f;
    }

    if (!g_animations_enabled) {
      state->cursor_blink.current = 1.0f;
      state->cursor_blink.target = 1.0f;
    } else {
      SmoothValue_Update(&state->cursor_blink, dt);
    }
  }

  /* Handle resizing */
  if (state->visible && state->last_bounds.w > 0) {
    rect resize_handle = {.x = state->last_bounds.x,
                          .y = state->last_bounds.y - 4,
                          .w = state->last_bounds.w,
                          .h = 8};

    v2i mouse_pos = Input_GetMousePos();
    b32 hover = UI_PointInRect(mouse_pos, resize_handle);

    if (!state->dragging && hover && Input_MousePressed(MOUSE_LEFT) &&
        ui->active == UI_ID_NONE) {
      state->dragging = true;
      ui->active = state->resizer_id;
      state->drag_start_y = (f32)mouse_pos.y;
      state->drag_start_ratio = state->height_ratio;
      state->drag_avail_height = available_height;
      Input_ConsumeMouse();
    }
  }

  if (state->dragging) {
    if (Input_MouseDown(MOUSE_LEFT)) {
      v2i mouse_pos = Input_GetMousePos();
      f32 dy = state->drag_start_y - (f32)mouse_pos.y;
      f32 h = (state->drag_avail_height > 1.0f) ? state->drag_avail_height
                                                : available_height;

      if (h > 1.0f) {
        f32 d_ratio = dy / h;
        state->height_ratio = state->drag_start_ratio + d_ratio;
        if (state->height_ratio < 0.1f)
          state->height_ratio = 0.1f;
        if (state->height_ratio > 0.8f)
          state->height_ratio = 0.8f;
      }
    } else {
      state->dragging = false;
      if (ui->active == state->resizer_id) {
        ui->active = UI_ID_NONE;
      }
    }
  }

  /* Handle mouse input for focus and selection */
  if (state->visible && state->anim.current > 0.5f &&
      state->last_bounds.w > 0) {
    v2i mouse_pos = Input_GetMousePos();
    b32 in_bounds = UI_PointInRect(mouse_pos, state->last_bounds);

    if (Input_MousePressed(MOUSE_LEFT) && in_bounds) {
      Input_SetFocus(INPUT_TARGET_TERMINAL);
      state->has_focus = true;

      /* Start selection */
      if (state->terminal) {
        /* Calculate cell coordinates */
        rect content = {
            .x = state->last_bounds.x + 4,
            .y = state->last_bounds.y + 8,
            .w = state->last_bounds.w - 8,
            .h = state->last_bounds.h - 8,
        };

        /* TODO: These should be synced with Render's cell_width/height
         * calculation */
        /* For now using defaults or measuring if mono_font were available here
         * globally */
        /* Actually we can approximate or use a better way.
           Let's use the same logic as in Render. */
        i32 cell_w = 8;
        i32 cell_h = 16;
        font *mono_font = ui->font;
        if (mono_font) {
          cell_w = Font_MeasureWidth(mono_font, "M");
          if (cell_w <= 0)
            cell_w = 8;
          cell_h = Font_GetLineHeight(mono_font);
          if (cell_h <= 0)
            cell_h = 16;
        }

        if (UI_PointInRect(mouse_pos, content)) {
          u32 tx = (u32)((mouse_pos.x - content.x) / cell_w);
          u32 ty = (u32)((mouse_pos.y - content.y) / cell_h);
          Terminal_StartSelection(state->terminal, tx, ty);
        } else {
          Terminal_ClearSelection(state->terminal);
        }
      }

      Input_ConsumeMouse();
    }

    if (Input_MouseDown(MOUSE_LEFT) && state->terminal &&
        state->terminal->is_selecting) {
      rect content = {
          .x = state->last_bounds.x + 4,
          .y = state->last_bounds.y + 8,
          .w = state->last_bounds.w - 8,
          .h = state->last_bounds.h - 8,
      };

      i32 cell_w = 8;
      i32 cell_h = 16;
      font *mono_font = ui->font;
      if (mono_font) {
        cell_w = Font_MeasureWidth(mono_font, "M");
        if (cell_w <= 0)
          cell_w = 8;
        cell_h = Font_GetLineHeight(mono_font);
        if (cell_h <= 0)
          cell_h = 16;
      }

      /* Smooth auto-scroll when selecting outside bounds */
      f32 scroll_delta = 0.0f;
      f32 scroll_speed = g_animations_enabled ? 10.0f : 100.0f;
      if (mouse_pos.y < content.y) {
        scroll_delta = (f32)(content.y - mouse_pos.y) * scroll_speed * dt;
      } else if (mouse_pos.y >= content.y + content.h) {
        scroll_delta =
            (f32)(content.y + content.h - 1 - mouse_pos.y) * scroll_speed * dt;
      }

      if (scroll_delta != 0.0f) {
        state->selection_scroll_accumulator += scroll_delta;
        i32 lines = (i32)state->selection_scroll_accumulator;
        if (lines != 0) {
          Terminal_Scroll(state->terminal, lines);
          state->selection_scroll_accumulator -= (f32)lines;
        }
      } else {
        state->selection_scroll_accumulator = 0.0f;
      }

      /* Clamp mouse to content area for selection coordinate calculation */
      i32 mx = mouse_pos.x;
      i32 my = mouse_pos.y;
      if (mx < content.x)
        mx = content.x;
      if (mx >= content.x + content.w)
        mx = content.x + content.w - 1;
      if (my < content.y)
        my = content.y;
      if (my >= content.y + content.h)
        my = content.y + content.h - 1;

      u32 tx = (u32)((mx - content.x) / cell_w);
      u32 ty = (u32)((my - content.y) / cell_h);

      /* Clamp tx to current columns */
      if (tx >= state->terminal->cols)
        tx = state->terminal->cols - 1;

      Terminal_MoveSelection(state->terminal, tx, ty);
    }

    if (Input_MouseReleased(MOUSE_LEFT) && state->terminal &&
        state->terminal->is_selecting) {
      Terminal_EndSelection(state->terminal);
    }
  }

  /* Skip if not visible and animation complete */
  if (!state->visible && state->anim.current < 0.01f)
    return;

  /* Update terminal (process PTY data) */
  if (state->terminal) {
    Terminal_Update(state->terminal);
  }

  /* Handle keyboard input when focused */
  if (state->has_focus && state->terminal &&
      Terminal_IsAlive(state->terminal) &&
      Input_HasFocus(INPUT_TARGET_TERMINAL)) {

    u32 mods = Input_GetModifiers();
    b32 at_eol = Terminal_IsCursorAtEOL(state->terminal);

    /* ===== Suggestion Acceptance ===== */
    /* Ctrl+F or Right Arrow (at EOL): Accept full suggestion */
    if ((Input_KeyPressed(KEY_RIGHT) && at_eol) ||
        (Input_KeyPressed(KEY_F) && (mods & MOD_CTRL))) {
      const char *suffix = Suggestion_GetSuffix(&state->current_suggestion);
      if (suffix && suffix[0] != '\0') {
        Terminal_Write(state->terminal, suffix, (u32)strlen(suffix));
        state->current_suggestion.valid = false;
      } else if (Input_KeyPressed(KEY_RIGHT)) {
        /* No suggestion, pass through normal right arrow */
        Terminal_Write(state->terminal, "\x1b[C", 3);
      }
    }
    /* Alt+F or Alt+Right: Accept first word of suggestion */
    else if ((Input_KeyPressed(KEY_RIGHT) || Input_KeyPressed(KEY_F)) &&
             (mods & MOD_ALT)) {
      char first_word[256];
      Suggestion_GetFirstWord(&state->current_suggestion, first_word,
                              sizeof(first_word));
      if (first_word[0] != '\0') {
        Terminal_Write(state->terminal, first_word, (u32)strlen(first_word));
        state->current_suggestion.valid = false;
      }
    }
    /* ===== Word Navigation & Deletion (Ctrl/Alt modifiers) ===== */
    /* Ctrl+Backspace: Delete word backward (send Ctrl+W) */
    else if (Input_KeyRepeat(KEY_BACKSPACE) && (mods & MOD_CTRL)) {
      Terminal_Write(state->terminal, "\x17",
                     1); /* Ctrl+W = backward-kill-word */
    }
    /* Alt+Backspace: Delete word backward (send ESC + DEL) */
    else if (Input_KeyRepeat(KEY_BACKSPACE) && (mods & MOD_ALT)) {
      Terminal_Write(state->terminal, "\x1b\x7f", 2);
    }
    /* Ctrl+Delete: Delete word forward */
    else if (Input_KeyRepeat(KEY_DELETE) && (mods & MOD_CTRL)) {
      Terminal_Write(state->terminal, "\x1b[3;5~", 6);
    }
    /* Ctrl+Left: Move word backward */
    else if (Input_KeyRepeat(KEY_LEFT) && (mods & MOD_CTRL)) {
      Terminal_Write(state->terminal, "\x1b[1;5D", 6);
    }
    /* Ctrl+Right: Move word forward */
    else if (Input_KeyRepeat(KEY_RIGHT) && (mods & MOD_CTRL)) {
      Terminal_Write(state->terminal, "\x1b[1;5C", 6);
    }
    /* Alt+Left: Move word backward (alternate) */
    else if (Input_KeyRepeat(KEY_LEFT) && (mods & MOD_ALT)) {
      Terminal_Write(state->terminal, "\x1b[1;3D", 6);
    }
    /* Alt+Right: Move word forward (alternate) */
    else if (Input_KeyRepeat(KEY_RIGHT) && (mods & MOD_ALT)) {
      Terminal_Write(state->terminal, "\x1b[1;3C", 6);
    }
    /* Ctrl+Home: Move to beginning of line/input */
    else if (Input_KeyRepeat(KEY_HOME) && (mods & MOD_CTRL)) {
      Terminal_Write(state->terminal, "\x1b[1;5H", 6);
    }
    /* Ctrl+End: Move to end of line/input */
    else if (Input_KeyRepeat(KEY_END) && (mods & MOD_CTRL)) {
      Terminal_Write(state->terminal, "\x1b[1;5F", 6);
    }
    /* ===== Standard Key Handling ===== */
    else if (Input_KeyRepeat(KEY_UP)) {
      Terminal_Write(state->terminal, "\x1b[A", 3);
    } else if (Input_KeyRepeat(KEY_DOWN)) {
      Terminal_Write(state->terminal, "\x1b[B", 3);
    } else if (Input_KeyRepeat(KEY_RIGHT)) {
      Terminal_Write(state->terminal, "\x1b[C", 3);
    } else if (Input_KeyRepeat(KEY_LEFT)) {
      Terminal_Write(state->terminal, "\x1b[D", 3);
    }

    if (Input_KeyRepeat(KEY_HOME)) {
      if (!(mods & MOD_CTRL)) { /* Already handled Ctrl+Home above */
        Terminal_Write(state->terminal, "\x1b[H", 3);
      }
    }
    if (Input_KeyRepeat(KEY_END)) {
      if (!(mods & MOD_CTRL)) { /* Already handled Ctrl+End above */
        Terminal_Write(state->terminal, "\x1b[F", 3);
      }
    }
    if (Input_KeyRepeat(KEY_PAGE_UP)) {
      if (mods & MOD_SHIFT) {
        /* Shift+PageUp = scroll terminal history */
        Terminal_Scroll(state->terminal, 10);
      } else {
        Terminal_Write(state->terminal, "\x1b[5~", 4);
      }
    }
    if (Input_KeyRepeat(KEY_PAGE_DOWN)) {
      if (mods & MOD_SHIFT) {
        Terminal_Scroll(state->terminal, -10);
      } else {
        Terminal_Write(state->terminal, "\x1b[6~", 4);
      }
    }
    if (Input_KeyRepeat(KEY_DELETE)) {
      if (!(mods & MOD_CTRL)) { /* Already handled Ctrl+Delete above */
        Terminal_Write(state->terminal, "\x1b[3~", 4);
      }
    }
    if (Input_KeyRepeat(KEY_BACKSPACE)) {
      if (!(mods & (MOD_CTRL |
                    MOD_ALT))) { /* Already handled Ctrl/Alt+Backspace above */
        Terminal_Write(state->terminal, "\x7f", 1);
      }
    }
    /* Handle tab completion override only if emulated mode AND suggestion is
     * active */
    if (Input_KeyPressed(KEY_TAB)) {
      b32 accepted = false;

      if (state->shell_mode == SHELL_EMULATED &&
          state->current_suggestion.valid && at_eol) {
        const char *suffix = Suggestion_GetSuffix(&state->current_suggestion);
        if (suffix && suffix[0] != '\0') {
          Terminal_Write(state->terminal, suffix, (u32)strlen(suffix));
          state->current_suggestion.valid = false;
          accepted = true;
        }
      }

      if (!accepted) {
        Terminal_Write(state->terminal, "\t", 1);
      }
    }
    if (Input_KeyPressed(KEY_RETURN)) {
      /* Record command to history before executing */
      const char *cmd = Terminal_GetCurrentLine(state->terminal);
      if (cmd && cmd[0] != '\0' && state->suggestions) {
        Suggestion_RecordCommand(state->suggestions, cmd);
      }
      Terminal_Write(state->terminal, "\r", 1);
      state->current_suggestion.valid = false;
    }
    if (Input_KeyPressed(KEY_ESCAPE)) {
      Terminal_Write(state->terminal, "\x1b", 1);
    }

    /* ===== Copy/Paste Shortcuts ===== */
    if (mods & MOD_CTRL) {
      /* Ctrl+C or Ctrl+Shift+C: Copy */
      if (Input_KeyPressed(KEY_C)) {
        if (state->terminal->has_selection) {
          char *text = Terminal_GetSelectionText(state->terminal);
          if (text) {
            Platform_SetClipboard(text);
            free(text);
            /* Maybe clear selection on copy? Terminal apps usually don't,
               but GUI apps do. Let's keep it highlighted. */
            Input_ConsumeKeys();
            goto skip_normal_input;
          }
        } else if (mods & MOD_SHIFT) {
          /* Shift+Ctrl+C is standard terminal copy */
          /* (Already handled by selection check above if text selected) */
        }
      }
      /* Ctrl+V or Ctrl+Shift+V: Paste */
      if (Input_KeyPressed(KEY_V)) {
        char buffer[4096];
        char *text = Platform_GetClipboard(buffer, sizeof(buffer));
        if (text) {
          Terminal_Write(state->terminal, text, (u32)strlen(text));
          Input_ConsumeKeys();
          goto skip_normal_input;
        }
      }
    }

    /* Ctrl+key combinations (except Ctrl+F which is handled above) */
    if (mods & MOD_CTRL) {
      for (int k = KEY_A; k <= KEY_Z; k++) {
        if (k == KEY_F)
          continue; /* Already handled for suggestion */
        if (Input_KeyPressed((key_code)k)) {
          char ctrl_char = (char)((k - KEY_A) + 1); /* Ctrl+A = 0x01, etc. */
          Terminal_Write(state->terminal, &ctrl_char, 1);
        }
      }
    }
    /* Regular text input */
    u32 text = Input_GetTextInput();
    if (text > 0 && !(mods & MOD_CTRL)) {
      char c = (char)text;
      Terminal_Write(state->terminal, &c, 1);
    }

    /* Mouse scroll for terminal history */
    f32 scroll = Input_GetScrollDelta();
    if (scroll != 0) {
      Terminal_Scroll(state->terminal, (i32)(scroll * 3));
      Input_ConsumeScroll();
    }

    /* Consume all keyboard input when terminal has focus */
    Input_ConsumeKeys();
    Input_ConsumeText();

  skip_normal_input:;
  }

  /* ===== Update Suggestions ===== */
  if (state->terminal && state->suggestions && state->visible) {
    const char *current_input = Terminal_GetCurrentLine(state->terminal);

    /* Update CWD for path suggestions */
    if (state->terminal && state->terminal->cwd[0] != '\0') {
      Suggestion_SetCWD(state->suggestions, state->terminal->cwd);
    } else if (state->last_cwd[0] != '\0') {
      Suggestion_SetCWD(state->suggestions, state->last_cwd);
    }

    /* Update suggestion if input changed or is non-empty */
    /* Only update suggestion if input changed or is non-empty */
    if (current_input) {
      if (current_input[0] != '\0' &&
          strcmp(current_input, state->last_input) != 0) {
        strncpy(state->last_input, current_input,
                sizeof(state->last_input) - 1);
        state->last_input[sizeof(state->last_input) - 1] = '\0';

        /* Get new suggestion only if in EMULATED mode */
        if (state->shell_mode == SHELL_EMULATED) {
          state->current_suggestion =
              Suggestion_Get(state->suggestions, current_input);
        } else {
          state->current_suggestion.valid = false;
        }
      } else if (current_input[0] == '\0') {
        /* Clear suggestion when input is empty */
        state->current_suggestion.valid = false;
        state->last_input[0] = '\0';
      }
    }
  }
}

void TerminalPanel_Render(terminal_panel_state *state, ui_context *ui,
                          rect bounds) {
  if (!state)
    return;

  /* Calculate panel height */
  f32 anim_value = state->anim.current;
  if (anim_value < 0.01f)
    return;

  i32 full_height = (i32)(bounds.h * state->height_ratio);
  i32 panel_height = (i32)((f32)full_height * anim_value);

  if (panel_height < 10)
    return;

  /* Panel appears at bottom of bounds */
  rect panel_bounds = {
      .x = bounds.x,
      .y = bounds.y + bounds.h - panel_height,
      .w = bounds.w,
      .h = panel_height,
  };

  /* Store for click detection in update */
  state->last_bounds = panel_bounds;

  render_context *r = ui->renderer;
  const theme *th = ui->theme;

  /* Draw panel background */
  color bg = {20, 20, 20, 255}; /* Darker than main bg for contrast */
  Render_DrawRect(r, panel_bounds, bg);

  /* Draw top border / resize handle */
  rect border = {panel_bounds.x, panel_bounds.y, panel_bounds.w, 2};
  color border_color = th->border;

  /* Check hover for render feedback */
  rect hit_rect = {panel_bounds.x, panel_bounds.y - 2, panel_bounds.w, 6};
  b32 hover_border = UI_PointInRect(ui->input.mouse_pos, hit_rect);

  if (state->dragging) {
    border_color = th->accent;
  } else if (hover_border) {
    border_color = th->accent_hover; /* Highlight on hover */
  } else if (state->has_focus) {
    border_color = th->accent;
  }
  Render_DrawRect(r, border, border_color);

  /* Terminal content area */
  i32 padding = 4;
  rect content = {
      .x = panel_bounds.x + padding,
      .y = panel_bounds.y + 4 + padding,
      .w = panel_bounds.w - padding * 2,
      .h = panel_bounds.h - 4 - padding * 2,
  };

  if (!state->terminal) {
    /* No terminal yet */
    color text_color = {128, 128, 128, 255};
    v2i pos = {content.x, content.y};
    /* Use ui->font directly as we haven't defined mono_font yet */
    Render_DrawText(r, pos, "Terminal not started", ui->font, text_color);
    return;
  }

  /* Calculate cell size based on font */
  font *mono_font = ui->font; /* TODO: Use dedicated mono font */
  i32 cell_width = 8;         /* Approximate monospace width */
  i32 cell_height = 16;       /* Line height */

  if (mono_font) {
    cell_width = Font_MeasureWidth(mono_font, "M");
    if (cell_width <= 0)
      cell_width = 8;
    cell_height = Font_GetLineHeight(mono_font);
    if (cell_height <= 0)
      cell_height = 16;
  }

  /* Calculate visible dimensions */
  u32 visible_cols = (u32)(content.w / cell_width);
  u32 visible_rows = (u32)(content.h / cell_height);

  /* Resize terminal if dimensions changed */
  if (state->terminal->cols != visible_cols ||
      state->terminal->rows != visible_rows) {
    if (visible_cols > 10 && visible_rows > 2) {
      Terminal_Resize(state->terminal, visible_cols, visible_rows);
    }
  }

  /* Render terminal cells */
  u32 rows = state->terminal->rows;
  u32 cols = state->terminal->cols;

  for (u32 y = 0; y < rows && y < visible_rows; y++) {
    for (u32 x = 0; x < cols && x < visible_cols; x++) {
      const terminal_cell *cell = Terminal_GetCell(state->terminal, x, y);
      if (!cell)
        continue;

      i32 px = content.x + (i32)x * cell_width;
      i32 py = content.y + (i32)y * cell_height;

      /* Get colors */
      cell_attr attr = cell->attr;
      color fg = get_ansi_color(attr.fg, true, attr.bold);
      color bg_cell = get_ansi_color(attr.bg, false, false);

      /* Handle reverse video */
      if (attr.reverse) {
        color tmp = fg;
        fg = bg_cell;
        bg_cell = tmp;
      }

      /* Draw background if not default OR selected */
      b32 selected = Terminal_IsCellSelected(state->terminal, x, y);
      if (attr.bg != TERM_DEFAULT_BG || attr.reverse || selected) {
        rect cell_rect = {px, py, cell_width, cell_height};
        color actual_bg = bg_cell;
        if (selected) {
          /* Highlight color: light blue with some transparency */
          actual_bg = (color){60, 120, 180, 180};
        }
        Render_DrawRect(r, cell_rect, actual_bg);
      }

      /* Draw cursor */
      if (Terminal_IsCursorAt(state->terminal, x, y) && state->has_focus) {
        b32 cursor_visible = state->cursor_blink.current > 0.5f;
        if (cursor_visible) {
          rect cursor_rect = {px, py, cell_width, cell_height};
          color cursor_color = th->accent;
          Render_DrawRect(r, cursor_rect, cursor_color);
          /* Invert text on cursor */
          fg = bg_cell;
        }
      }

      /* Draw character */
      u32 cp = cell->codepoint;
      if (cp > 32 && cp < 127) {
        char str[2] = {(char)cp, '\0'};
        v2i char_pos = {px, py};
        Render_DrawText(r, char_pos, str, mono_font, fg);
      } else if (cp > 127) {
        char str[2] = {'?', '\0'};
        v2i char_pos = {px, py};
        Render_DrawText(r, char_pos, str, mono_font, fg);
      }
    }
  }

  /* ===== Render Suggestion Ghost Text ===== */
  if (state->current_suggestion.valid && state->has_focus) {
    const char *suffix = Suggestion_GetSuffix(&state->current_suggestion);
    if (suffix && suffix[0] != '\0') {
      /* Get cursor position */
      u32 cursor_x = Terminal_GetCursorCol(state->terminal);
      u32 cursor_y = state->terminal->cursor_y;

      /* Only show suggestion at end of line */
      if (Terminal_IsCursorAtEOL(state->terminal)) {
        i32 ghost_x = content.x + (i32)cursor_x * cell_width;
        i32 ghost_y = content.y + (i32)cursor_y * cell_height;

        /* Gray color for ghost text */
        color ghost_color = {100, 100, 100, 220};

        v2i ghost_pos = {ghost_x, ghost_y};
        Render_DrawText(r, ghost_pos, suffix, mono_font, ghost_color);
      }
    }
  }

  /* Draw "dead" indicator if process exited */
  if (!Terminal_IsAlive(state->terminal)) {
    const char *msg = "[Process exited]";
    if (mono_font) {
      i32 msg_width = Font_MeasureWidth(mono_font, msg);
      i32 msg_x = content.x + content.w - msg_width - 10;
      i32 msg_y = content.y;
      color warn = {255, 200, 100, 255};
      v2i msg_pos = {msg_x, msg_y};
      Render_DrawText(r, msg_pos, msg, mono_font, warn);
    }
  }

  /* ===== Scrollbar ===== */
  if (state->terminal->scrollback_count > 0) {
    i32 sb_width = 12;
    rect sb_track = {
        .x = panel_bounds.x + panel_bounds.w - sb_width - 2,
        .y = content.y,
        .w = sb_width,
        .h = content.h,
    };

    /* Draw track background (subtle) */
    color track_color = {30, 30, 30, 255};
    Render_DrawRect(r, sb_track, track_color);

    /* Calculate thumb */
    u32 total_lines = state->terminal->scrollback_count + state->terminal->rows;
    u32 viewport_lines = state->terminal->rows;

    /* Avoid div by zero */
    if (total_lines > 0) {
      f32 view_ratio = (f32)viewport_lines / (f32)total_lines;
      i32 thumb_height = (i32)((f32)sb_track.h * view_ratio);
      if (thumb_height < 20)
        thumb_height = 20;

      /* Scroll offset 0 = bottom, offset max = top */
      /* Invert logic for visual scrollbar (top=0) */
      /* When scroll_offset = 0 (bottom), thumb should be at bottom of track */
      /* When scroll_offset = max (top), thumb should be at top of track */

      i32 scrollable_height = sb_track.h - thumb_height;
      f32 scroll_ratio = (f32)state->terminal->scroll_offset /
                         (f32)state->terminal->scrollback_count;

      /* Clamp ratio */
      if (scroll_ratio > 1.0f)
        scroll_ratio = 1.0f;
      if (scroll_ratio < 0.0f)
        scroll_ratio = 0.0f;

      /* Calculate y position.
         Ratio 1.0 (top of history) -> y = 0
         Ratio 0.0 (bottom of history) -> y = max_y
      */
      i32 thumb_y =
          sb_track.y + (i32)((1.0f - scroll_ratio) * (f32)scrollable_height);

      rect sb_thumb = {
          .x = sb_track.x + 2,
          .y = thumb_y,
          .w = sb_width - 4,
          .h = thumb_height,
      };

      color thumb_color = {80, 80, 80, 255};
      if (state->has_focus) {
        thumb_color = (color){100, 100, 100, 255};
      }

      Render_DrawRect(r, sb_thumb, thumb_color);
    }
  }
}

i32 TerminalPanel_GetHeight(terminal_panel_state *state, i32 available_height) {
  if (!state)
    return 0;

  f32 anim_value = state->anim.current;
  if (anim_value < 0.01f)
    return 0;

  i32 full_height = (i32)((f32)available_height * state->height_ratio);
  return (i32)((f32)full_height * anim_value);
}

b32 TerminalPanel_IsVisible(terminal_panel_state *state) {
  if (!state)
    return false;
  return state->visible || state->anim.current > 0.01f;
}

void TerminalPanel_Focus(terminal_panel_state *state) {
  if (state) {
    state->has_focus = true;
  }
}

b32 TerminalPanel_HasFocus(terminal_panel_state *state) {
  if (!state)
    return false;
  return state->has_focus && state->visible;
}
