/*
 * layout.h - Flexible panel layout system
 *
 * Manages the layout of the application, including panels, splitters,
 * and different layout modes.
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#include "../core/task_queue.h"
#include "../core/types.h"
#include "../renderer/renderer.h"
#include "components/drag_drop.h"
#include "components/explorer.h"
#include "components/progress_bar.h"
#include "components/scroll_container.h"
#include "components/terminal_panel.h"
#include "ui.h"

typedef enum {
  WB_LAYOUT_MODE_SINGLE,
  WB_LAYOUT_MODE_DUAL,
} layout_mode;

/* Forward declaration */
struct context_menu_state_s;

typedef struct {
  explorer_state explorer;
  terminal_panel_state terminal;
  bool active;
} panel;

typedef struct layout_state_s {
  memory_arena *arena;
  layout_mode mode;
  panel panels[2];
  u32 active_panel_idx;

  /* Splitter state */
  f32 split_ratio;        /* 0.0 - 1.0 (current animated value) */
  f32 target_split_ratio; /* Target for animation */
  bool dragging;          /* Is the splitter being dragged? */
  f32 drag_start_x;       /* Mouse X when drag started */
  f32 drag_start_ratio;   /* Split ratio when drag started */

  /* Global Modals */
  bool show_config_diagnostics;
  scroll_container_state diagnostic_scroll;

  /* Drag and Drop */
  drag_drop_state drag_drop;

  /* Shared Clipboard (for cross-panel copy/paste) */
  char clipboard_paths[EXPLORER_MAX_CLIPBOARD][FS_MAX_PATH];
  i32 clipboard_count;
  b32 clipboard_is_cut;

  /* Context menu reference (managed by main.c) */
  struct context_menu_state_s *context_menu;

  /* Background Task Queue */
  task_queue tasks;

  /* Progress Bar */
  progress_bar_state progress_bar;
} layout_state;

#define MIN_PANEL_WIDTH 100.0f

/* Initialize the layout system */
void Layout_Init(layout_state *layout, memory_arena *arena);

/* Shutdown the layout system */
void Layout_Shutdown(layout_state *layout);

/* Update layout logic (input handling, animations) - call before render */
void Layout_Update(layout_state *layout, ui_context *ui, rect bounds);

/* Update settings from config */
void Layout_RefreshConfig(layout_state *layout);

/* Render the layout - pure rendering, no state changes */
void Layout_Render(layout_state *layout, ui_context *ui, rect bounds);

/* Control functions */
void Layout_SetMode(layout_state *layout, layout_mode mode);
void Layout_ToggleMode(layout_state *layout);
void Layout_SetActivePanel(layout_state *layout, u32 index);
panel *Layout_GetActivePanel(layout_state *layout);

/* Toggle terminal for active panel */
void Layout_ToggleTerminal(layout_state *layout);

#endif /* LAYOUT_H */
