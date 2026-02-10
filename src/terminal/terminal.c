/*
 * terminal.c - Terminal emulator implementation
 *
 * Handles terminal buffer management, ANSI sequence processing,
 * and PTY communication with a dedicated read thread.
 * C99, handmade hero style.
 */

#include "terminal.h"
#include "../platform/platform.h"

#include <stdlib.h>
#include <string.h>

#define READ_BUFFER_SIZE (64 * 1024) /* 64KB ring buffer */

/* ===== Helper Functions ===== */

static void reset_attr(cell_attr *attr) {
  attr->fg = TERM_DEFAULT_FG;
  attr->bg = TERM_DEFAULT_BG;
  attr->bold = 0;
  attr->dim = 0;
  attr->italic = 0;
  attr->underline = 0;
  attr->blink = 0;
  attr->reverse = 0;
  attr->hidden = 0;
  attr->strikethrough = 0;
}

static void clear_cell(terminal_cell *cell) {
  cell->codepoint = ' ';
  reset_attr(&cell->attr);
}

static void clear_line(Terminal *term, u32 y) {
  if (y >= term->rows)
    return;
  for (u32 x = 0; x < term->cols; x++) {
    clear_cell(&term->screen[y * term->cols + x]);
  }
}

static void clear_screen_cells(Terminal *term) {
  for (u32 i = 0; i < term->cols * term->rows; i++) {
    clear_cell(&term->screen[i]);
  }
}

/* ===== Scroll Operations ===== */

static void push_line_to_scrollback(Terminal *term, u32 line) {
  if (!term->scrollback)
    return;

  u32 dest_line =
      (term->scrollback_start + term->scrollback_count) % term->scrollback_size;

  memcpy(&term->scrollback[dest_line * term->cols],
         &term->screen[line * term->cols], term->cols * sizeof(terminal_cell));

  if (term->scrollback_count < term->scrollback_size) {
    term->scrollback_count++;
  } else {
    term->scrollback_start =
        (term->scrollback_start + 1) % term->scrollback_size;
  }
}

static void scroll_up(Terminal *term, u32 top, u32 bottom, u32 count) {
  if (top >= bottom || count == 0)
    return;

  /* Push lines to scrollback if we're scrolling from top */
  if (top == 0) {
    for (u32 i = 0; i < count && i < bottom; i++) {
      push_line_to_scrollback(term, i);
    }
  }

  /* Move lines up */
  u32 lines_to_move = bottom - top - count;
  if (lines_to_move > 0) {
    memmove(&term->screen[top * term->cols],
            &term->screen[(top + count) * term->cols],
            lines_to_move * term->cols * sizeof(terminal_cell));
  }

  /* Clear bottom lines */
  for (u32 i = 0; i < count && (bottom - 1 - i) >= top; i++) {
    clear_line(term, bottom - 1 - i);
  }

  term->dirty = true;
}

static void scroll_down(Terminal *term, u32 top, u32 bottom, u32 count) {
  if (top >= bottom || count == 0)
    return;

  /* Move lines down */
  u32 lines_to_move = bottom - top - count;
  if (lines_to_move > 0) {
    memmove(&term->screen[(top + count) * term->cols],
            &term->screen[top * term->cols],
            lines_to_move * term->cols * sizeof(terminal_cell));
  }

  /* Clear top lines */
  for (u32 i = 0; i < count; i++) {
    clear_line(term, top + i);
  }

  term->dirty = true;
}

/* ===== Read Thread ===== */

static void *pty_read_thread(void *arg) {
  Terminal *term = (Terminal *)arg;
  char buffer[4096];

  while (term->thread_running && PTY_IsAlive(term->pty)) {
    i32 bytes = PTY_Read(term->pty, buffer, sizeof(buffer));

    if (bytes > 0) {
      pthread_mutex_lock(&term->buffer_mutex);

      /* Write to ring buffer */
      for (i32 i = 0; i < bytes; i++) {
        u32 next_head = (term->read_head + 1) % term->read_buffer_size;
        if (next_head != term->read_tail) {
          term->read_buffer[term->read_head] = buffer[i];
          term->read_head = next_head;
        }
        /* If buffer full, drop oldest data (overwrite) */
      }

      pthread_mutex_unlock(&term->buffer_mutex);
    } else if (bytes < 0) {
      /* Error, exit thread */
      break;
    } else {
      /* No data, sleep briefly */
      Platform_SleepMs(5);
    }
  }

  return NULL;
}

/* ===== ANSI Sequence Handling ===== */

static void handle_sgr(Terminal *term, i32 *params, i32 count) {
  if (count == 0) {
    /* ESC[m = reset */
    reset_attr(&term->current_attr);
    return;
  }

  for (i32 i = 0; i < count; i++) {
    i32 code = params[i];

    switch (code) {
    case 0:
      reset_attr(&term->current_attr);
      break;
    case 1:
      term->current_attr.bold = 1;
      break;
    case 2:
      term->current_attr.dim = 1;
      break;
    case 3:
      term->current_attr.italic = 1;
      break;
    case 4:
      term->current_attr.underline = 1;
      break;
    case 5:
    case 6:
      term->current_attr.blink = 1;
      break;
    case 7:
      term->current_attr.reverse = 1;
      break;
    case 8:
      term->current_attr.hidden = 1;
      break;
    case 9:
      term->current_attr.strikethrough = 1;
      break;
    case 22:
      term->current_attr.bold = 0;
      term->current_attr.dim = 0;
      break;
    case 23:
      term->current_attr.italic = 0;
      break;
    case 24:
      term->current_attr.underline = 0;
      break;
    case 25:
      term->current_attr.blink = 0;
      break;
    case 27:
      term->current_attr.reverse = 0;
      break;
    case 28:
      term->current_attr.hidden = 0;
      break;
    case 29:
      term->current_attr.strikethrough = 0;
      break;

    /* Foreground colors (30-37) */
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
      term->current_attr.fg = (u8)(code - 30);
      break;
    case 39:
      term->current_attr.fg = TERM_DEFAULT_FG;
      break;
    /* Bright foreground colors (90-97) */
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
      term->current_attr.fg = (u8)((code - 90) + 8);
      break;

    /* Background colors (40-47) */
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
      term->current_attr.bg = (u8)(code - 40);
      break;
    case 49:
      term->current_attr.bg = TERM_DEFAULT_BG;
      break;
    /* Bright background colors (100-107) */
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
      term->current_attr.bg = (u8)((code - 100) + 8);
      break;

    /* 256 color mode: 38;5;N or 48;5;N */
    case 38:
      if (i + 2 < count && params[i + 1] == 5) {
        term->current_attr.fg = (u8)params[i + 2];
        i += 2;
      }
      break;
    case 48:
      if (i + 2 < count && params[i + 1] == 5) {
        term->current_attr.bg = (u8)params[i + 2];
        i += 2;
      }
      break;
    }
  }
}

static void handle_csi(Terminal *term, ansi_result *result) {
  i32 *params = result->data.csi.params;
  i32 count = result->data.csi.param_count;
  char cmd = result->data.csi.command;

  /* Default parameter is 1 for most commands */
  i32 n = (count > 0 && params[0] > 0) ? params[0] : 1;
  i32 m = (count > 1 && params[1] > 0) ? params[1] : 1;

  switch (cmd) {
  case 'c': /* DA - Device Attributes */
    /* Respond with VT100 + Advanced Video Option (AVO) */
    /* Sequence: ESC [ ? 1 ; 2 c */
    Terminal_Write(term, "\x1b[?1;2c", 7);
    break;

  case 'A': /* CUU - Cursor Up */
    term->cursor_y = (term->cursor_y >= (u32)n) ? term->cursor_y - n : 0;
    break;

  case 'B': /* CUD - Cursor Down */
    term->cursor_y += n;
    if (term->cursor_y >= term->rows)
      term->cursor_y = term->rows - 1;
    break;

  case 'C': /* CUF - Cursor Forward */
    term->cursor_x += n;
    if (term->cursor_x >= term->cols)
      term->cursor_x = term->cols - 1;
    break;

  case 'D': /* CUB - Cursor Back */
    term->cursor_x = (term->cursor_x >= (u32)n) ? term->cursor_x - n : 0;
    break;

  case 'E': /* CNL - Cursor Next Line */
    term->cursor_x = 0;
    term->cursor_y += n;
    if (term->cursor_y >= term->rows)
      term->cursor_y = term->rows - 1;
    break;

  case 'F': /* CPL - Cursor Previous Line */
    term->cursor_x = 0;
    term->cursor_y = (term->cursor_y >= (u32)n) ? term->cursor_y - n : 0;
    break;

  case 'G': /* CHA - Cursor Horizontal Absolute */
    term->cursor_x = (n > 0) ? n - 1 : 0;
    if (term->cursor_x >= term->cols)
      term->cursor_x = term->cols - 1;
    break;

  case 'H': /* CUP - Cursor Position */
  case 'f': /* HVP - same as CUP */
    term->cursor_y = (n > 0) ? n - 1 : 0;
    term->cursor_x = (m > 0) ? m - 1 : 0;
    if (term->cursor_y >= term->rows)
      term->cursor_y = term->rows - 1;
    if (term->cursor_x >= term->cols)
      term->cursor_x = term->cols - 1;
    break;

  case 'J': /* ED - Erase Display */
    n = (count > 0) ? params[0] : 0;
    if (n == 0) {
      /* Clear from cursor to end */
      for (u32 x = term->cursor_x; x < term->cols; x++) {
        clear_cell(&term->screen[term->cursor_y * term->cols + x]);
      }
      for (u32 y = term->cursor_y + 1; y < term->rows; y++) {
        clear_line(term, y);
      }
    } else if (n == 1) {
      /* Clear from start to cursor */
      for (u32 y = 0; y < term->cursor_y; y++) {
        clear_line(term, y);
      }
      for (u32 x = 0; x <= term->cursor_x; x++) {
        clear_cell(&term->screen[term->cursor_y * term->cols + x]);
      }
    } else if (n == 2 || n == 3) {
      /* Clear entire screen */
      clear_screen_cells(term);
      /* Clear scrollback too for both 2 (ED) and 3 (Clear History) */
      /* This is a user-preference choice to match "Ctrl+L cleans buffer"
       * request */
      term->scrollback_count = 0;
      term->scrollback_start = 0;
      term->scroll_offset = 0;
    }
    break;

  case 'K': /* EL - Erase Line */
    n = (count > 0) ? params[0] : 0;
    if (n == 0) {
      /* Clear from cursor to end of line */
      for (u32 x = term->cursor_x; x < term->cols; x++) {
        clear_cell(&term->screen[term->cursor_y * term->cols + x]);
      }
    } else if (n == 1) {
      /* Clear from start to cursor */
      for (u32 x = 0; x <= term->cursor_x && x < term->cols; x++) {
        clear_cell(&term->screen[term->cursor_y * term->cols + x]);
      }
    } else if (n == 2) {
      /* Clear entire line */
      clear_line(term, term->cursor_y);
    }
    break;

  case 'S': /* SU - Scroll Up */
    scroll_up(term, term->scroll_top, term->scroll_bottom, n);
    break;

  case 'T': /* SD - Scroll Down */
    scroll_down(term, term->scroll_top, term->scroll_bottom, n);
    break;

  case 'm': /* SGR - Select Graphic Rendition */
    handle_sgr(term, params, count);
    break;

  case 's': /* SCP - Save Cursor Position */
    term->saved_cursor_x = term->cursor_x;
    term->saved_cursor_y = term->cursor_y;
    break;

  case 'u': /* RCP - Restore Cursor Position */
    term->cursor_x = term->saved_cursor_x;
    term->cursor_y = term->saved_cursor_y;
    break;

  case 'r': /* DECSTBM - Set Top and Bottom Margins */
    term->scroll_top = (count > 0 && params[0] > 0) ? (u32)(params[0] - 1) : 0;
    term->scroll_bottom =
        (count > 1 && params[1] > 0) ? (u32)params[1] : term->rows;
    if (term->scroll_top >= term->rows)
      term->scroll_top = 0;
    if (term->scroll_bottom > term->rows)
      term->scroll_bottom = term->rows;
    if (term->scroll_top >= term->scroll_bottom) {
      term->scroll_top = 0;
      term->scroll_bottom = term->rows;
    }
    term->cursor_x = 0;
    term->cursor_y = 0;
    break;

  case 'h': /* SM - Set Mode */
  case 'l': /* RM - Reset Mode */
    /* Handle cursor visibility */
    if (result->data.csi.private_mode && count > 0 && params[0] == 25) {
      term->cursor_visible = (cmd == 'h');
    }
    break;

  case '@': /* ICH - Insert Character */
    /* Shift characters right */
    if (term->cursor_x < term->cols) {
      u32 to_move = term->cols - term->cursor_x - n;
      if (to_move > 0) {
        memmove(&term->screen[term->cursor_y * term->cols + term->cursor_x + n],
                &term->screen[term->cursor_y * term->cols + term->cursor_x],
                to_move * sizeof(terminal_cell));
      }
      for (i32 i = 0; i < n && term->cursor_x + i < term->cols; i++) {
        clear_cell(
            &term->screen[term->cursor_y * term->cols + term->cursor_x + i]);
      }
    }
    break;

  case 'P': /* DCH - Delete Character */
    if (term->cursor_x < term->cols) {
      u32 to_move = term->cols - term->cursor_x - n;
      if (to_move > 0) {
        memmove(&term->screen[term->cursor_y * term->cols + term->cursor_x],
                &term->screen[term->cursor_y * term->cols + term->cursor_x + n],
                to_move * sizeof(terminal_cell));
      }
      for (u32 x = term->cols - n; x < term->cols; x++) {
        clear_cell(&term->screen[term->cursor_y * term->cols + x]);
      }
    }
    break;

  case 'L': /* IL - Insert Lines */
    if (term->cursor_y >= term->scroll_top &&
        term->cursor_y < term->scroll_bottom) {
      scroll_down(term, term->cursor_y, term->scroll_bottom, n);
    }
    break;

  case 'M': /* DL - Delete Lines */
    if (term->cursor_y >= term->scroll_top &&
        term->cursor_y < term->scroll_bottom) {
      scroll_up(term, term->cursor_y, term->scroll_bottom, n);
    }
    break;
  }

  term->dirty = true;
}

static void handle_control(Terminal *term, u8 c) {
  switch (c) {
  case '\r': /* CR - Carriage Return */
    term->cursor_x = 0;
    break;

  case '\n': /* LF - Line Feed */
  case '\v': /* VT - Vertical Tab */
  case '\f': /* FF - Form Feed */
    term->cursor_y++;
    if (term->cursor_y >= term->scroll_bottom) {
      term->cursor_y = term->scroll_bottom - 1;
      scroll_up(term, term->scroll_top, term->scroll_bottom, 1);
    }
    break;

  case '\t': /* HT - Horizontal Tab */
    term->cursor_x = ((term->cursor_x / 8) + 1) * 8;
    if (term->cursor_x >= term->cols)
      term->cursor_x = term->cols - 1;
    break;

  case '\b': /* BS - Backspace */
    if (term->cursor_x > 0)
      term->cursor_x--;
    break;

  case '\a': /* BEL - Bell */
    /* Could trigger system beep */
    break;

  case 0x0E: /* SO - Shift Out (G1 charset) */
  case 0x0F: /* SI - Shift In (G0 charset) */
    /* Character set switching - ignore for now */
    break;
  }

  term->dirty = true;
}

static void put_char(Terminal *term, u32 codepoint) {
  if (term->cursor_x >= term->cols) {
    /* Line wrap */
    /* Mark the last character of the previous line as wrapped */
    u32 prev_y = term->cursor_y;
    u32 prev_x = term->cols - 1;
    terminal_cell *last_cell = &term->screen[prev_y * term->cols + prev_x];
    last_cell->attr.wrapped = 1;

    term->cursor_x = 0;
    term->cursor_y++;
    if (term->cursor_y >= term->scroll_bottom) {
      term->cursor_y = term->scroll_bottom - 1;
      scroll_up(term, term->scroll_top, term->scroll_bottom, 1);
    }
  }

  terminal_cell *cell =
      &term->screen[term->cursor_y * term->cols + term->cursor_x];
  cell->codepoint = codepoint;
  cell->attr = term->current_attr;
  cell->attr.wrapped = 0; /* Clear wrap flag for new char */

  term->cursor_x++;
  term->dirty = true;
}

static void process_byte(Terminal *term, u8 byte) {
  ansi_result result = ANSI_Parse(&term->parser, byte);

  switch (result.action) {
  case WB_ANSI_ACTION_PRINT:
    put_char(term, result.data.character);
    break;

  case WB_ANSI_ACTION_EXECUTE:
    handle_control(term, result.data.control);
    break;

  case WB_ANSI_ACTION_CSI:
    handle_csi(term, &result);
    break;

  case WB_ANSI_ACTION_OSC:
    /* Handle window title (OSC 0 or 2) */
    if (result.data.osc) {
      const char *text = result.data.osc;
      if ((text[0] == '0' || text[0] == '2') && text[1] == ';') {
        strncpy(term->title, text + 2, sizeof(term->title) - 1);
        term->title[sizeof(term->title) - 1] = '\0';
      }
      /* Handle CWD notification (OSC 7) - Standard semantic integration */
      /* Format: 7;file://hostname/path/to/cwd */
      else if (text[0] == '7' && text[1] == ';') {
        const char *url = text + 2;
        if (strncmp(url, "file://", 7) == 0) {
          const char *path = url + 7;
          /* Skip hostname if present */
          const char *slash = strchr(path, '/');
          if (slash) {
            strncpy(term->cwd, slash, sizeof(term->cwd) - 1);
            term->cwd[sizeof(term->cwd) - 1] = '\0';
          }
        }
      }
    }
    break;

  case WB_ANSI_ACTION_DCS:
    /* DCS sequences handled (and ignored/consumed) by parser */
    break;

  case WB_ANSI_ACTION_NONE:
    break;
  }
}

/* ===== Public API ===== */

Terminal *Terminal_Create(u32 cols, u32 rows) {
  Terminal *term = malloc(sizeof(Terminal));
  if (!term)
    return NULL;

  memset(term, 0, sizeof(Terminal));

  term->cols = cols;
  term->rows = rows;
  term->scroll_top = 0;
  term->scroll_bottom = rows;
  term->cursor_visible = true;

  /* Initialize selection */
  term->is_selecting = false;
  term->has_selection = false;
  memset(&term->sel_start, 0, sizeof(terminal_coord));
  memset(&term->sel_end, 0, sizeof(terminal_coord));

  /* Allocate screen buffer */
  term->screen = malloc(cols * rows * sizeof(terminal_cell));
  if (!term->screen) {
    free(term);
    return NULL;
  }
  clear_screen_cells(term);

  /* Allocate scrollback */
  term->scrollback_size = TERMINAL_SCROLLBACK_LINES;
  term->scrollback =
      malloc(term->scrollback_size * cols * sizeof(terminal_cell));
  if (!term->scrollback) {
    /* Continue without scrollback */
    term->scrollback_size = 0;
  }

  /* Allocate read buffer */
  term->read_buffer_size = READ_BUFFER_SIZE;
  term->read_buffer = malloc(term->read_buffer_size);
  if (!term->read_buffer) {
    free(term->scrollback);
    free(term->screen);
    free(term);
    return NULL;
  }

  /* Initialize mutex */
  pthread_mutex_init(&term->buffer_mutex, NULL);

  /* Initialize parser */
  ANSI_Init(&term->parser);

  /* Initialize default attributes */
  reset_attr(&term->current_attr);

  term->dirty = true;

  return term;
}

void Terminal_Destroy(Terminal *term) {
  if (!term)
    return;

  /* Stop read thread */
  if (term->thread_running) {
    term->thread_running = false;
    pthread_join(term->read_thread, NULL);
  }

  /* Destroy PTY */
  if (term->pty) {
    PTY_Destroy(term->pty);
  }

  pthread_mutex_destroy(&term->buffer_mutex);

  free(term->read_buffer);
  free(term->scrollback);
  free(term->screen);
  free(term);
}

b32 Terminal_Spawn(Terminal *term, const char *shell, const char *cwd) {
  if (!term)
    return false;

  /* Kill existing PTY if any */
  if (term->pty) {
    if (term->thread_running) {
      term->thread_running = false;
      pthread_join(term->read_thread, NULL);
    }
    PTY_Destroy(term->pty);
    term->pty = NULL;
  }

  /* Create new PTY */
  term->pty = PTY_Create(shell, cwd);
  if (!term->pty)
    return false;

  /* Set PTY size */
  PTY_Resize(term->pty, term->cols, term->rows);

  /* Start read thread */
  term->thread_running = true;
  term->read_head = 0;
  term->read_tail = 0;

  if (pthread_create(&term->read_thread, NULL, pty_read_thread, term) != 0) {
    term->thread_running = false;
    PTY_Destroy(term->pty);
    term->pty = NULL;
    return false;
  }

  return true;
}

b32 Terminal_IsAlive(Terminal *term) {
  if (!term || !term->pty)
    return false;
  return PTY_IsAlive(term->pty);
}

void Terminal_Update(Terminal *term) {
  if (!term)
    return;

  /* Process pending data from read thread */
  pthread_mutex_lock(&term->buffer_mutex);

  while (term->read_tail != term->read_head) {
    u8 byte = term->read_buffer[term->read_tail];
    term->read_tail = (term->read_tail + 1) % term->read_buffer_size;

    /* DEBUG: Log removed */

    /* Release lock briefly to allow reader thread to continue */
    pthread_mutex_unlock(&term->buffer_mutex);
    process_byte(term, byte);
    pthread_mutex_lock(&term->buffer_mutex);
  }

  pthread_mutex_unlock(&term->buffer_mutex);

  /* Do NOT reset scroll to bottom on update - allows reading history while
   * output arrives */
  if (term->dirty && term->scroll_offset > 0) {
    /* logic removed */
  }
}

void Terminal_Resize(Terminal *term, u32 cols, u32 rows) {
  if (!term || cols == 0 || rows == 0)
    return;

  if (cols == term->cols && rows == term->rows)
    return;

  /* O(N) Strategy:
     1. Allocate a dynamic "builder" buffer to hold the ENTIRE new state
     (lines). We don't know the exact new line count, but it scales roughly
     inverse to width. Let's start with capacity = current_total_lines *
     old_cols / new_cols * 1.5 + cushion */

  /* Trim trailing empty lines from screen to prevent preserving "void" */
  u32 screen_rows_used = 0;
  for (i32 y = (i32)term->rows - 1; y >= 0; y--) {
    b32 empty = true;
    for (u32 x = 0; x < term->cols; x++) {
      terminal_cell *c = &term->screen[y * term->cols + x];
      if (c->codepoint != 0 && c->codepoint != ' ') {
        empty = false;
        break;
      }
      if (c->attr.bg != TERM_DEFAULT_BG) {
        empty = false;
        break;
      }
    }
    if (!empty) {
      screen_rows_used = (u32)(y + 1);
      break;
    }
  }

  /* Ensure we keep at least up to the cursor */
  if (term->cursor_y >= screen_rows_used) {
    screen_rows_used = term->cursor_y + 1;
  }

  u32 total_old_lines = term->scrollback_count + screen_rows_used;
  u32 estimated_capacity =
      (total_old_lines * term->cols) / cols + total_old_lines + 100;

  /* We use a temporary struct to hold pointers to rows to avoid copying large
     blocks twice. However, to be "zero memmove" for the *re-ingest*, we want to
     append new rows directly to a growing list. */

  typedef struct {
    terminal_cell *cells; /* cols elements */
    u32 len;              /* Always cols actually, but good for debug */
  } Line;

  Line *lines = malloc(estimated_capacity * sizeof(Line));
  u32 line_count = 0;
  u32 line_capacity = estimated_capacity;

  u32 new_cursor_x = 0;
  u32 new_cursor_y = 0; /* Index in 'lines' initially */
  b32 cursor_found = false;

  b32 sel_start_found = false;
  b32 sel_end_found = false;
  u32 new_sel_start_x = 0, new_sel_start_y = 0;
  u32 new_sel_end_x = 0, new_sel_end_y = 0;

  /* Buffer for current logical line (unwrapped) */
  u32 logical_cap = 4096;
  terminal_cell *logical_line = malloc(logical_cap * sizeof(terminal_cell));

  u32 i = 0;
  while (i < total_old_lines) {
    /* 2. Stream Logical Lines (Merge Wrapped) */
    u32 logical_len = 0;
    b32 line_has_cursor = false;
    u32 cursor_offset = 0;

    b32 line_has_sel_start = false;
    b32 line_has_sel_end = false;
    u32 sel_start_offset = 0;
    u32 sel_end_offset = 0;

    /* Collect consecutive wrapped lines into `logical_line` */
    while (i < total_old_lines) {
      /* Get source row */
      terminal_cell *src_row;
      b32 row_is_cursor = false;
      b32 row_is_sel_start = false;
      b32 row_is_sel_end = false;

      if (i < term->scrollback_count) {
        u32 idx = (term->scrollback_start + i) % term->scrollback_size;
        src_row = &term->scrollback[idx * term->cols];
      } else {
        u32 screen_y = i - term->scrollback_count;
        src_row = &term->screen[screen_y * term->cols];
        if (screen_y == term->cursor_y)
          row_is_cursor = true;
      }

      if (term->has_selection) {
        if (i == term->sel_start.y)
          row_is_sel_start = true;
        if (i == term->sel_end.y)
          row_is_sel_end = true;
      }

      /* Check wrap on last cell */
      terminal_cell *last_cell = &src_row[term->cols - 1];
      b32 is_wrapped = last_cell->attr.wrapped;

      /* Determine copy length */
      u32 copy_len = term->cols;

      if (!is_wrapped) {
        /* Trim empty space from non-wrapped lines */
        while (copy_len > 0) {
          terminal_cell *c = &src_row[copy_len - 1];
          /* Treat NULL or empty space with default BG as empty */
          if (c->codepoint != 0 && c->codepoint != ' ')
            break;
          if (c->attr.bg != TERM_DEFAULT_BG || c->attr.reverse)
            break;
          copy_len--;
        }
      }

      /* Expand logical buffer if needed */
      if (logical_len + copy_len > logical_cap) {
        logical_cap = (logical_len + copy_len > logical_cap * 2)
                          ? (logical_len + copy_len + 1024)
                          : (logical_cap * 2);
        logical_line =
            realloc(logical_line, logical_cap * sizeof(terminal_cell));
      }

      /* Coordinate Checks (ensure copy covers coordinate if it was in the
       * whitespace) */
      if (row_is_cursor) {
        if (copy_len <= term->cursor_x)
          copy_len = term->cursor_x + 1;
        line_has_cursor = true;
        cursor_offset = logical_len + term->cursor_x;
      }
      if (row_is_sel_start) {
        if (copy_len <= term->sel_start.x)
          copy_len = term->sel_start.x + 1;
        line_has_sel_start = true;
        sel_start_offset = logical_len + term->sel_start.x;
      }
      if (row_is_sel_end) {
        if (copy_len <= term->sel_end.x)
          copy_len = term->sel_end.x + 1;
        line_has_sel_end = true;
        sel_end_offset = logical_len + term->sel_end.x;
      }

      /* Copy to logical */
      for (u32 k = 0; k < copy_len; k++) {
        logical_line[logical_len + k] = src_row[k];
        logical_line[logical_len + k].attr.wrapped = 0; /* Clear wrap flag */
      }
      logical_len += copy_len;

      i++;
      if (!is_wrapped)
        break;
    }

    /* Ensure empty explicit lines exist as 1-height (unless completely empty
     * logical split?) */
    if (logical_len == 0) {
      logical_len = 1;
      clear_cell(&logical_line[0]);
    }

    /* 3. Reflow Logical Line into New Chunks */
    u32 processed = 0;
    while (processed < logical_len) {
      u32 chunk_len = cols;
      if (processed + chunk_len > logical_len) {
        chunk_len = logical_len - processed;
      }

      /* Allocate New Line */
      if (line_count >= line_capacity) {
        line_capacity *= 2;
        lines = realloc(lines, line_capacity * sizeof(Line));
      }

      terminal_cell *new_row = malloc(cols * sizeof(terminal_cell));
      for (u32 c = 0; c < cols; c++)
        clear_cell(&new_row[c]);

      /* Copy Content */
      for (u32 k = 0; k < chunk_len; k++) {
        new_row[k] = logical_line[processed + k];
      }

      /* Set Wrap Flag */
      if (chunk_len == cols && processed + chunk_len < logical_len) {
        new_row[cols - 1].attr.wrapped = 1;
      }

      /* Track Coordinates */
      if (line_has_cursor && !cursor_found) {
        if (cursor_offset >= processed &&
            cursor_offset < processed + chunk_len) {
          new_cursor_x = cursor_offset - processed;
          new_cursor_y = line_count;
          cursor_found = true;
        }
      }
      if (line_has_sel_start && !sel_start_found) {
        if (sel_start_offset >= processed &&
            sel_start_offset < processed + chunk_len) {
          new_sel_start_x = sel_start_offset - processed;
          new_sel_start_y = line_count;
          sel_start_found = true;
        }
      }
      if (line_has_sel_end && !sel_end_found) {
        if (sel_end_offset >= processed &&
            sel_end_offset < processed + chunk_len) {
          new_sel_end_x = sel_end_offset - processed;
          new_sel_end_y = line_count;
          sel_end_found = true;
        }
      }

      lines[line_count].cells = new_row;
      lines[line_count].len = cols;
      line_count++;

      processed += chunk_len;
    }
  }
  free(logical_line);

  /* 4. Commit to Terminal State */
  /* We have 'line_count' lines in 'lines'. Need to split into Scrollback +
   * Screen. */

  u32 final_sb_count = 0;

  /* Determine start index for screen */
  u32 screen_start_idx = 0;

  if (line_count > rows) {
    /* Full screen + some scrollback */
    screen_start_idx = line_count - rows;
    final_sb_count = screen_start_idx;
  } else {
    /* Fits entirely in screen (at top) */
    screen_start_idx = 0;
    /* Rest is empty padding */
    final_sb_count = 0;
  }

  /* === SCROLLBACK === */
  /* If we have more history than max size, trim top */
  u32 sb_lines_to_keep = final_sb_count;
  u32 sb_source_start = 0;

  if (sb_lines_to_keep > term->scrollback_size && term->scrollback_size > 0) {
    sb_source_start = sb_lines_to_keep - term->scrollback_size;
    sb_lines_to_keep = term->scrollback_size;
  }

  /* Reallocate Scrollback Buffer */
  if (term->scrollback)
    free(term->scrollback);
  term->scrollback = NULL;

  if (term->scrollback_size > 0) {
    term->scrollback =
        malloc(term->scrollback_size * cols * sizeof(terminal_cell));
    /* Copy lines into ring buffer (linear fill is fine since we just reset
     * start=0) */
    u32 dst_idx = 0;
    for (u32 k = 0; k < sb_lines_to_keep; k++) {
      terminal_cell *src = lines[sb_source_start + k].cells;
      terminal_cell *dst = &term->scrollback[dst_idx * cols];
      memcpy(dst, src, cols * sizeof(terminal_cell));
      dst_idx++;
    }
    term->scrollback_count = sb_lines_to_keep;
    term->scrollback_start = 0;
  } else {
    term->scrollback_count = 0;
    term->scrollback_start = 0;
  }

  /* === SCREEN === */
  /* New Screen Buffer */
  terminal_cell *new_screen = malloc(rows * cols * sizeof(terminal_cell));
  for (u32 x = 0; x < rows * cols; x++)
    clear_cell(&new_screen[x]);

  /* Copy applicable lines to screen */
  /* If line_count < rows, we fill 0..line_count */
  /* If line_count >= rows, we fill screen_start_idx..line_count */

  u32 screen_fill_count = (line_count > rows) ? rows : line_count;
  /* source starts at screen_start_idx */

  for (u32 k = 0; k < screen_fill_count; k++) {
    terminal_cell *src = lines[screen_start_idx + k].cells;
    terminal_cell *dst = &new_screen[k * cols];
    memcpy(dst, src, cols * sizeof(terminal_cell));
  }

  /* Replace Screen */
  if (term->screen)
    free(term->screen);
  term->screen = new_screen;

  /* Update Cursor */
  /* new_cursor_y is absolute index in 'lines'. map to screen. */
  if (cursor_found) {
    if (new_cursor_y >= screen_start_idx) {
      term->cursor_y = new_cursor_y - screen_start_idx;
      term->cursor_x = new_cursor_x;
    } else {
      /* Cursor is in allowed scrollback?
         Actually standard terms might pull it to top?
         Let's clamp to 0,0 if it scrolled off. */
      term->cursor_y = 0;
      term->cursor_x = new_cursor_x;
      /* If we support viewport following cursor? */
    }
  } else {
    /* Default to top left if lost? or bottom left? */
    term->cursor_x = 0;
    term->cursor_y = (screen_fill_count > 0) ? screen_fill_count - 1 : 0;
  }

  /* Update selection positions (absolute virtual indices) */
  if (term->has_selection) {
    /* If either end was lost (not found in current flow), clear selection */
    if (sel_start_found && sel_end_found) {
      term->sel_start.x = new_sel_start_x;
      term->sel_start.y = new_sel_start_y;
      term->sel_end.x = new_sel_end_x;
      term->sel_end.y = new_sel_end_y;
    } else {
      term->has_selection = false;
    }
  }

  /* CLEANUP */
  for (u32 k = 0; k < line_count; k++) {
    if (lines[k].cells)
      free(lines[k].cells);
  }
  free(lines);

  term->cols = cols;
  term->rows = rows;
  term->scroll_offset = 0;
  term->scroll_top = 0;
  term->scroll_bottom = rows;

  /* Clamp cursor */
  if (term->cursor_x >= cols)
    term->cursor_x = cols - 1;
  if (term->cursor_y >= rows)
    term->cursor_y = rows - 1;

  if (term->pty) {
    PTY_Resize(term->pty, cols, rows);
  }

  term->dirty = true;
}

void Terminal_Write(Terminal *term, const char *data, u32 size) {
  if (!term || !term->pty || !data || size == 0)
    return;

  PTY_Write(term->pty, data, size);

  /* User input snaps to bottom */
  if (term->scroll_offset > 0) {
    term->scroll_offset = 0;
    term->dirty = true;
  }
}

void Terminal_SendKey(Terminal *term, u32 keycode, u32 modifiers) {
  if (!term || !term->pty)
    return;

  const char *seq = NULL;
  char buf[8];

  /* Standard key mappings to escape sequences */
  switch (keycode) {
  case 0x01: /* KEY_UP (assuming platform defines) */
    seq = "\x1b[A";
    break;
  case 0x02: /* KEY_DOWN */
    seq = "\x1b[B";
    break;
  case 0x03: /* KEY_RIGHT */
    seq = "\x1b[C";
    break;
  case 0x04: /* KEY_LEFT */
    seq = "\x1b[D";
    break;
  case 0x05: /* KEY_HOME */
    seq = "\x1b[H";
    break;
  case 0x06: /* KEY_END */
    seq = "\x1b[F";
    break;
  case 0x07: /* KEY_PAGE_UP */
    seq = "\x1b[5~";
    break;
  case 0x08: /* KEY_PAGE_DOWN */
    seq = "\x1b[6~";
    break;
  case 0x09: /* KEY_DELETE */
    seq = "\x1b[3~";
    break;
  case 0x0A: /* KEY_INSERT */
    seq = "\x1b[2~";
    break;
  default:
    /* For regular keys, caller should use Terminal_Write */
    return;
  }

  if (seq) {
    Terminal_Write(term, seq, strlen(seq));
  }
  (void)buf;
  (void)modifiers;
}

void Terminal_Scroll(Terminal *term, i32 lines) {
  if (!term)
    return;

  term->scroll_offset += lines;

  /* Clamp to valid range */
  i32 max_scroll = (i32)term->scrollback_count;
  if (term->scroll_offset > max_scroll)
    term->scroll_offset = max_scroll;
  if (term->scroll_offset < 0)
    term->scroll_offset = 0;

  term->dirty = true;
}

const terminal_cell *Terminal_GetCell(Terminal *term, u32 x, u32 y) {
  if (!term || x >= term->cols || y >= term->rows)
    return NULL;

  /* Calculate virtual line index (0 = oldest scrollback line) */
  /* Virtual height = scrollback_count + rows */
  /* Viewport starts at: scrollback_count - scroll_offset */

  i32 virtual_line =
      (i32)term->scrollback_count - (i32)term->scroll_offset + (i32)y;

  if (virtual_line < 0) {
    /* Viewing before start of history (shouldn't happen with clamped offset) */
    return NULL;
  }

  if (virtual_line < (i32)term->scrollback_count) {
    /* Line is in scrollback */
    u32 actual_line =
        (term->scrollback_start + virtual_line) % term->scrollback_size;
    return &term->scrollback[actual_line * term->cols + x];
  } else {
    /* Line is in screen */
    u32 screen_y = (u32)(virtual_line - (i32)term->scrollback_count);
    if (screen_y < term->rows) {
      return &term->screen[screen_y * term->cols + x];
    }
  }

  return NULL;
}

b32 Terminal_IsCursorAt(Terminal *term, u32 x, u32 y) {
  if (!term)
    return false;

  /* Cursor only visible when not scrolled back */
  if (term->scroll_offset > 0)
    return false;

  return term->cursor_visible && term->cursor_x == x && term->cursor_y == y;
}

const char *Terminal_GetTitle(Terminal *term) {
  if (!term)
    return "";
  return term->title[0] ? term->title : "Terminal";
}

void Terminal_Clear(Terminal *term) {
  if (!term)
    return;

  clear_screen_cells(term);
  term->cursor_x = 0;
  term->cursor_y = 0;
  term->scroll_offset = 0;

  /* Clear selection on clear screen */
  term->is_selecting = false;
  term->has_selection = false;

  term->dirty = true;
}

const char *Terminal_GetCWD(Terminal *term) {
  /* This is hard to get accurately without cooperation from shell */
  /* For now, just return NULL - the panel will use explorer's CWD */
  (void)term;
  return NULL;
}

/* ===== Suggestion Support ===== */

/*
 * Detect where the prompt ends on the current line.
 * Looks for common prompt characters: $, #, >, ❯, %, →
 * Returns the column after the prompt, or 0 if not found.
 */
static u32 detect_prompt_end(Terminal *term) {
  if (!term)
    return 0;

  u32 y = term->cursor_y;
  u32 last_prompt_pos = 0;

  /* Scan the line for prompt characters */
  for (u32 x = 0; x < term->cols && x < term->cursor_x; x++) {
    terminal_cell *cell = &term->screen[y * term->cols + x];
    u32 cp = cell->codepoint;

    /* Common prompt endings */
    if (cp == '$' || cp == '#' || cp == '>' || cp == '%') {
      /* Check if next char is space (common pattern: "$ " or "> ") */
      if (x + 1 < term->cols) {
        terminal_cell *next = &term->screen[y * term->cols + x + 1];
        if (next->codepoint == ' ') {
          last_prompt_pos = x + 2; /* After "$ " */
        } else {
          last_prompt_pos = x + 1; /* After "$" */
        }
      } else {
        last_prompt_pos = x + 1;
      }
    }
    /* Unicode prompt symbols: ❯ (276F), → (2192), ➜ (279C) */
    else if (cp == 0x276F || cp == 0x2192 || cp == 0x279C) {
      if (x + 1 < term->cols) {
        terminal_cell *next = &term->screen[y * term->cols + x + 1];
        if (next->codepoint == ' ') {
          last_prompt_pos = x + 2;
        } else {
          last_prompt_pos = x + 1;
        }
      } else {
        last_prompt_pos = x + 1;
      }
    }
  }

  return last_prompt_pos;
}

const char *Terminal_GetCurrentLine(Terminal *term) {
  if (!term)
    return NULL;

  /* Detect where prompt ends */
  u32 prompt_end = detect_prompt_end(term);
  term->prompt_end_col = prompt_end;

  /* Extract text from prompt end to cursor */
  u32 y = term->cursor_y;
  u32 len = 0;

  for (u32 x = prompt_end; x < term->cursor_x && x < term->cols; x++) {
    terminal_cell *cell = &term->screen[y * term->cols + x];
    u32 cp = cell->codepoint;

    if (cp == 0 || (cp == ' ' && len == 0)) {
      /* Skip leading spaces and nulls (only if at start of input) */
      if (len == 0)
        continue;
      /* Otherwise fallthrough to add space */
    }

    /* Add character to buffer (simple conversion) */
    if (len < sizeof(term->current_line) - 1) {
      if (cp < 128) {
        term->current_line[len++] = (char)cp;
      } else {
        /* Replace non-ASCII with ? for now to keep length correct/safe */
        /* TODO: Real UTF-8 would be better but requires more logic */
        term->current_line[len++] = '?';
      }
    }
  }

  term->current_line[len] = '\0';
  term->current_line_len = len;

  return term->current_line;
}

b32 Terminal_IsCursorAtEOL(Terminal *term) {
  if (!term)
    return false;

  u32 y = term->cursor_y;
  u32 x = term->cursor_x;

  /* Check if cursor is at end of content on this line */
  /* Look at cell at cursor position - if it's empty/space, we're at EOL */
  if (x >= term->cols)
    return true;

  terminal_cell *cell = &term->screen[y * term->cols + x];

  /* If current cell is empty (null or space), we're at end of input */
  if (cell->codepoint == 0 || cell->codepoint == ' ') {
    /* But verify there's no content after cursor */
    for (u32 cx = x + 1; cx < term->cols; cx++) {
      terminal_cell *c = &term->screen[y * term->cols + cx];
      if (c->codepoint != 0 && c->codepoint != ' ') {
        return false; /* Content after cursor, not at EOL */
      }
    }
    return true;
  }

  return false;
}

/* ===== Selection Implementation ===== */

static i32 get_virtual_line(Terminal *term, u32 y) {
  /* y is viewport relative (0 = top of visible screen) */
  /* Scroll offset 0 = bottom, offset max = top */
  return (i32)term->scrollback_count - (i32)term->scroll_offset + (i32)y;
}

void Terminal_StartSelection(Terminal *term, u32 x, u32 y) {
  if (!term)
    return;
  term->sel_start.x = x;
  term->sel_start.y = (u32)get_virtual_line(term, y);
  term->sel_end = term->sel_start;
  term->is_selecting = true;
  term->has_selection = true; /* Selection exists as soon as we start */
  term->dirty = true;
}

void Terminal_MoveSelection(Terminal *term, u32 x, u32 y) {
  if (!term || !term->is_selecting)
    return;
  term->sel_end.x = x;
  term->sel_end.y = (u32)get_virtual_line(term, y);
  term->dirty = true;
}

void Terminal_EndSelection(Terminal *term) {
  if (!term)
    return;
  term->is_selecting = false;
  /* If start == end, we can clear it if we want click-to-deselect,
     but usually we keep the highlight until next interaction. */
  if (term->sel_start.x == term->sel_end.x &&
      term->sel_start.y == term->sel_end.y) {
    term->has_selection = false;
  }
}

void Terminal_ClearSelection(Terminal *term) {
  if (!term)
    return;
  term->is_selecting = false;
  term->has_selection = false;
  term->dirty = true;
}

b32 Terminal_IsCellSelected(Terminal *term, u32 x, u32 y) {
  if (!term || !term->has_selection)
    return false;

  i32 vy = get_virtual_line(term, y);
  terminal_coord start = term->sel_start;
  terminal_coord end = term->sel_end;

  /* Normalize start/end so start is always before/above end */
  if (start.y > end.y || (start.y == end.y && start.x > end.x)) {
    terminal_coord tmp = start;
    start = end;
    end = tmp;
  }

  if (vy < (i32)start.y || vy > (i32)end.y)
    return false;
  if (vy > (i32)start.y && vy < (i32)end.y)
    return true;

  if (start.y == end.y) {
    return (u32)x >= start.x && (u32)x <= end.x;
  }

  if (vy == (i32)start.y)
    return (u32)x >= start.x;
  if (vy == (i32)end.y)
    return (u32)x <= end.x;

  return false;
}

char *Terminal_GetSelectionText(Terminal *term) {
  if (!term || !term->has_selection)
    return NULL;

  terminal_coord start = term->sel_start;
  terminal_coord end = term->sel_end;

  /* Normalize */
  if (start.y > end.y || (start.y == end.y && start.x > end.x)) {
    terminal_coord tmp = start;
    start = end;
    end = tmp;
  }

  /* Estimate buffer size: (rows * cols * 4 bytes for UTF-8) + newlines */
  u32 num_lines = end.y - start.y + 1;
  usize capacity = num_lines * (term->cols + 1) * 4 + 1;
  char *buffer = malloc(capacity);
  if (!buffer)
    return NULL;

  usize len = 0;
  for (u32 y = start.y; y <= end.y; y++) {
    u32 x_start = (y == start.y) ? start.x : 0;
    u32 x_end = (y == end.y) ? end.x : term->cols - 1;

    /* Clamp x_end to current terminal width */
    if (x_end >= term->cols)
      x_end = term->cols - 1;

    /* Get line data */
    const terminal_cell *row_data = NULL;
    if (y < term->scrollback_count) {
      u32 idx = (term->scrollback_start + y) % term->scrollback_size;
      row_data = &term->scrollback[idx * term->cols];
    } else {
      u32 screen_y = y - term->scrollback_count;
      if (screen_y < term->rows) {
        row_data = &term->screen[screen_y * term->cols];
      }
    }

    if (!row_data)
      continue;

    b32 line_wrapped = row_data[term->cols - 1].attr.wrapped;

    /* Extract text from cells */
    for (u32 x = x_start; x <= x_end; x++) {
      u32 cp = row_data[x].codepoint;
      if (cp == 0)
        cp = ' ';

      /* Simple UTF-8 encoding */
      if (cp < 0x80) {
        buffer[len++] = (char)cp;
      } else if (cp < 0x800) {
        buffer[len++] = (char)(0xC0 | (cp >> 6));
        buffer[len++] = (char)(0x80 | (cp & 0x3F));
      } else if (cp < 0x10000) {
        buffer[len++] = (char)(0xE0 | (cp >> 12));
        buffer[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buffer[len++] = (char)(0x80 | (cp & 0x3F));
      } else {
        buffer[len++] = (char)(0xF0 | (cp >> 18));
        buffer[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buffer[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buffer[len++] = (char)(0x80 | (cp & 0x3F));
      }
    }

    /* Add newline if not end of selection AND not wrapped line */
    if (y < end.y && !line_wrapped) {
      buffer[len++] = '\n';
    }
  }

  buffer[len] = '\0';
  return buffer;
}

u32 Terminal_GetCursorCol(Terminal *term) {
  if (!term)
    return 0;
  return term->cursor_x;
}
