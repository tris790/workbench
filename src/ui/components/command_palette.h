/*
 * command_palette.h - Command Palette for Workbench
 *
 * VSCode-style command palette for quick access to files and commands.
 * Ctrl+P: File search, Ctrl+Shift+P: Command mode (prefix >)
 * C99, handmade hero style.
 */

#ifndef COMMAND_PALETTE_H
#define COMMAND_PALETTE_H

#include "../../core/fs.h"
#include "../ui.h"

/* ===== Configuration ===== */

#define PALETTE_MAX_INPUT 256
#define PALETTE_MAX_ITEMS 128
#define PALETTE_MAX_COMMANDS 64
#define PALETTE_MAX_SHORTCUT 32
#define PALETTE_MAX_RECENT_COMMANDS 2

/* ===== Types ===== */

typedef enum {
  WB_PALETTE_MODE_CLOSED = 0,
  WB_PALETTE_MODE_FILE,    /* Ctrl+P - file search */
  WB_PALETTE_MODE_COMMAND, /* Ctrl+Shift+P - command mode */
} palette_mode;

/* Command callback function pointer */
typedef void (*command_callback)(void *user_data);

/* Individual palette item (file or command) */
typedef struct {
  char label[256];
  char shortcut[PALETTE_MAX_SHORTCUT]; /* e.g. "Ctrl + H" */
  char category[64];                   /* e.g. "commonly used" */
  char tags[256];                      /* e.g. "settings config options" */
  file_icon_type icon;
  command_callback callback;
  void *user_data;
  b32 is_file;
  i32 command_index; /* Index into registered commands array */
  i32 match_score;   /* Fuzzy match score for sorting (higher = better) */
} palette_item;

/* Registered command */
typedef struct {
  char name[256];
  char shortcut[PALETTE_MAX_SHORTCUT];
  char category[64];
  char tags[256];
  command_callback callback;
  void *user_data;
} palette_command;

/* Palette state */
typedef struct {
  /* Mode */
  palette_mode mode;

  /* Input */
  char input_buffer[PALETTE_MAX_INPUT];
  ui_text_state input_state;

  /* Items (filtered results) */
  palette_item items[PALETTE_MAX_ITEMS];
  i32 item_count;
  i32 selected_index;

  /* Scroll */
  ui_scroll_state scroll;
  b32 scroll_to_selection;

  /* Animation */
  smooth_value fade_anim;
  b32 just_opened;

  /* Registered commands */
  palette_command commands[PALETTE_MAX_COMMANDS];
  i32 command_count;

  /* Recently used commands (indices) */
  i32 recent_commands[PALETTE_MAX_RECENT_COMMANDS];
  i32 recent_count;

  /* File system reference (for file search) */
  fs_state *fs;

  /* Cached dimensions */
  i32 item_height;
  rect panel_bounds;
} command_palette_state;

/* ===== Command Palette API ===== */

/* Initialize palette state */
void CommandPalette_Init(command_palette_state *state, fs_state *fs);

/* Register a command */
void CommandPalette_RegisterCommand(command_palette_state *state,
                                    const char *name, const char *shortcut,
                                    const char *category, const char *tags,
                                    command_callback callback, void *user_data);

/* Open palette in specified mode */
void CommandPalette_Open(command_palette_state *state, palette_mode mode);

/* Close palette */
void CommandPalette_Close(command_palette_state *state);

/* Check if palette is open */
b32 CommandPalette_IsOpen(command_palette_state *state);

/* Update palette (handles input, returns true if consuming input) */
b32 CommandPalette_Update(command_palette_state *state, ui_context *ui);

/* Render palette overlay */
void CommandPalette_Render(command_palette_state *state, ui_context *ui,
                           i32 win_width, i32 win_height);

#endif /* COMMAND_PALETTE_H */
