/*
 * terminal_panel.h - Terminal panel UI component for Workbench
 *
 * Renders and manages the integrated terminal panel.
 * C99, handmade hero style.
 */

#ifndef TERMINAL_PANEL_H
#define TERMINAL_PANEL_H

#include "animation.h"
#include "terminal.h"
#include "suggestion.h"
#include "types.h"
#include "ui.h"

/* Shell operation mode */
typedef enum {
  SHELL_FISH = 0,      /* Use 'fish' shell (no emulation) */
  SHELL_SYSTEM,        /* Use system default shell (no emulation) */
  SHELL_EMULATED       /* Use system default shell + Workbench emulation */
} ShellMode;

/* Terminal panel state (one per explorer panel as per user request) */
typedef struct {
  Terminal *terminal;    /* Terminal instance (lazy-created) */
  b32 visible;           /* Is panel currently visible */
  b32 has_focus;         /* Does terminal have keyboard focus */
  f32 height_ratio;      /* Height as ratio of available space (0.0-1.0) */
  smooth_value anim;     /* Animation for slide in/out */
  smooth_value cursor_blink;
  char last_cwd[512];    /* Last CWD used to spawn terminal */
  rect last_bounds;      /* Panel bounds from last render (for click detection) */
  
  ShellMode shell_mode;  /* Current shell mode */
  
  /* Suggestion engine state */
  SuggestionEngine *suggestions;
  Suggestion current_suggestion;
  char last_input[1024]; /* Last input we generated suggestion for */
} terminal_panel_state;


/* Initialize terminal panel state */
void TerminalPanel_Init(terminal_panel_state *state);

/* Destroy terminal panel and free resources */
void TerminalPanel_Destroy(terminal_panel_state *state);

/* Toggle terminal visibility. If opening and no terminal exists, spawn one.
 * cwd: Working directory for new terminal (from explorer)
 */
void TerminalPanel_Toggle(terminal_panel_state *state, const char *cwd);

/* Update terminal state (call each frame) */
void TerminalPanel_Update(terminal_panel_state *state, ui_context *ui, f32 dt, b32 is_active);

/* Render terminal panel in given bounds (bottom portion of bounds) */
void TerminalPanel_Render(terminal_panel_state *state, ui_context *ui,
                          rect bounds);

/* Get height of terminal panel (for layout calculations) */
i32 TerminalPanel_GetHeight(terminal_panel_state *state, i32 available_height);

/* Check if terminal is visible */
b32 TerminalPanel_IsVisible(terminal_panel_state *state);

/* Focus the terminal for keyboard input */
void TerminalPanel_Focus(terminal_panel_state *state);

/* Check if terminal has focus */
b32 TerminalPanel_HasFocus(terminal_panel_state *state);

#endif /* TERMINAL_PANEL_H */
