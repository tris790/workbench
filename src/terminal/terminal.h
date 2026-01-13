/*
 * terminal.h - Terminal emulator core for Workbench
 *
 * Manages terminal buffer, cursor, and ANSI sequence processing.
 * C99, handmade hero style.
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include "../core/types.h"
#include "ansi_parser.h"
#include "workbench_pty.h"
#include <pthread.h>

/* Terminal coordinate (absolute) */
typedef struct {
  u32 x;
  u32 y; /* Absolute virtual line index (0 = oldest scrollback line) */
} terminal_coord;

/* Terminal cell attributes */
typedef struct {
  u8 fg; /* Foreground color index (0-15 for basic, 16-255 for extended) */
  u8 bg; /* Background color index */
  u8 bold : 1;
  u8 dim : 1;
  u8 italic : 1;
  u8 underline : 1;
  u8 blink : 1;
  u8 reverse : 1;
  u8 hidden : 1;
  u8 strikethrough : 1;
  u8 wrapped : 1;
} cell_attr;

/* Default attributes */
#define TERM_DEFAULT_FG 7 /* White */
#define TERM_DEFAULT_BG 0 /* Black */

/* Single cell in terminal buffer */
typedef struct {
  u32 codepoint;  /* Unicode codepoint (UTF-32) */
  cell_attr attr; /* Visual attributes */
} terminal_cell;

/* Terminal scroll buffer configuration */
#define TERMINAL_SCROLLBACK_LINES 1000

typedef struct Terminal Terminal;

struct Terminal {
  /* Screen buffer (current visible portion) */
  terminal_cell *screen;
  u32 cols;
  u32 rows;

  /* Scrollback buffer (ring buffer of lines) */
  terminal_cell *scrollback;
  u32 scrollback_size;  /* Number of lines in scrollback */
  u32 scrollback_start; /* Start index in ring buffer */
  u32 scrollback_count; /* Number of valid lines */

  /* Scroll offset (0 = at bottom, showing live output) */
  i32 scroll_offset;

  /* Cursor state */
  u32 cursor_x;
  u32 cursor_y;
  b32 cursor_visible;
  u32 saved_cursor_x;
  u32 saved_cursor_y;

  /* Current attributes for new characters */
  cell_attr current_attr;

  /* Scroll region (for CSI r command) */
  u32 scroll_top;
  u32 scroll_bottom;

  /* PTY connection */
  PTY *pty;

  /* ANSI parser */
  ansi_parser parser;

  /* Read thread for PTY (per user feedback) */
  pthread_t read_thread;
  pthread_mutex_t buffer_mutex;
  b32 thread_running;

  /* Ring buffer for incoming data from read thread */
  char *read_buffer;
  u32 read_buffer_size;
  u32 read_head;
  u32 read_tail;

  /* Terminal title (from OSC) */
  char title[256];

  /* Dirty flag for rendering optimization */
  b32 dirty;

  /* Selection state */
  terminal_coord sel_start;
  terminal_coord sel_end;
  b32 is_selecting;
  b32 has_selection;

  /* Current input line extraction (for suggestions) */
  char current_line[1024]; /* Extracted current input line */
  u32 current_line_len;
  u32 prompt_end_col; /* Estimated column where prompt ends */

  char cwd[512]; /* Current working directory (from OSC 7) */
};

/* Initialize terminal with given dimensions */
Terminal *Terminal_Create(u32 cols, u32 rows);

/* Destroy terminal and free resources */
void Terminal_Destroy(Terminal *term);

/* Start shell process in given working directory */
b32 Terminal_Spawn(Terminal *term, const char *shell, const char *cwd);

/* Check if terminal process is still running */
b32 Terminal_IsAlive(Terminal *term);

/* Process incoming data from PTY (call from main thread) */
void Terminal_Update(Terminal *term);

/* Resize terminal to new dimensions */
void Terminal_Resize(Terminal *term, u32 cols, u32 rows);

/* Write keyboard input to terminal */
void Terminal_Write(Terminal *term, const char *data, u32 size);

/* Write a single key (handles special keys like arrows, Home, End) */
void Terminal_SendKey(Terminal *term, u32 keycode, u32 modifiers);

/* Scroll by given number of lines (positive = up, negative = down) */
void Terminal_Scroll(Terminal *term, i32 lines);

/* Get cell at given position (handles scroll offset) */
const terminal_cell *Terminal_GetCell(Terminal *term, u32 x, u32 y);

/* Check if cursor should be visible at given position */
b32 Terminal_IsCursorAt(Terminal *term, u32 x, u32 y);

/* Get terminal title */
const char *Terminal_GetTitle(Terminal *term);

/* Clear screen */
void Terminal_Clear(Terminal *term);

/* Get CWD from shell (best effort, may not be accurate) */
const char *Terminal_GetCWD(Terminal *term);

/* ===== Suggestion Support ===== */

/* Extract current input line from cursor row (text after prompt) */
const char *Terminal_GetCurrentLine(Terminal *term);

/* Check if cursor is at end of input (for suggestion acceptance) */
b32 Terminal_IsCursorAtEOL(Terminal *term);

/* Get cursor column position */
u32 Terminal_GetCursorCol(Terminal *term);

/* ===== Selection API ===== */

/* Start selection at given viewport coordinates */
void Terminal_StartSelection(Terminal *term, u32 x, u32 y);

/* Update selection end point at given viewport coordinates */
void Terminal_MoveSelection(Terminal *term, u32 x, u32 y);

/* End selection */
void Terminal_EndSelection(Terminal *term);

/* Clear selection */
void Terminal_ClearSelection(Terminal *term);

/* Check if cell at viewport coordinates is selected */
b32 Terminal_IsCellSelected(Terminal *term, u32 x, u32 y);

/* Get selected text as a null-terminated string (caller must free) */
char *Terminal_GetSelectionText(Terminal *term);

#endif /* TERMINAL_H */
