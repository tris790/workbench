/*
 * layout.h - Flexible panel layout system
 *
 * Manages the layout of the application, including panels, splitters,
 * and different layout modes.
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#include "explorer.h"
#include "renderer.h"
#include "types.h"
#include "ui.h"

typedef enum {
  LAYOUT_MODE_SINGLE,
  LAYOUT_MODE_DUAL,
} layout_mode;

typedef struct {
  explorer_state explorer;
  bool active;
} panel;

typedef struct {
  layout_mode mode;
  panel panels[2];
  u32 active_panel_idx;

  /* Splitter state */
  f32 split_ratio;        /* 0.0 - 1.0 (current animated value) */
  f32 target_split_ratio; /* Target for animation */
  bool dragging;          /* Is the splitter being dragged? */
  f32 drag_start_x;       /* Mouse X when drag started */
  f32 drag_start_ratio;   /* Split ratio when drag started */
} layout_state;

#define MIN_PANEL_WIDTH 100.0f

/* Initialize the layout system */
void Layout_Init(layout_state *layout, memory_arena *arena);

/* Update layout logic (input handling, animations) - call before render */
void Layout_Update(layout_state *layout, ui_context *ui, rect bounds);

/* Render the layout - pure rendering, no state changes */
void Layout_Render(layout_state *layout, ui_context *ui, rect bounds);

/* Control functions */
void Layout_SetMode(layout_state *layout, layout_mode mode);
void Layout_ToggleMode(layout_state *layout);
void Layout_SetActivePanel(layout_state *layout, u32 index);
panel *Layout_GetActivePanel(layout_state *layout);

#endif /* LAYOUT_H */
