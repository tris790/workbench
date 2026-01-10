/*
 * layout.c - Flexible panel layout system implementation
 */

#include "layout.h"

#include <string.h>

#define SPLITTER_WIDTH 4.0f
#define ANIMATION_SPEED 12.0f

/* Cached splitter ID to avoid hashing every frame */
static ui_id g_splitter_id = 0;

void Layout_Init(layout_state *layout, memory_arena *arena) {
  layout->mode = LAYOUT_MODE_SINGLE;
  layout->active_panel_idx = 0;
  layout->split_ratio = 0.5f;
  layout->target_split_ratio = 0.5f;
  layout->dragging = false;

  /* Initialize panels */
  Explorer_Init(&layout->panels[0].explorer, arena);
  Explorer_Init(&layout->panels[1].explorer, arena);

  layout->panels[0].active = true;
  layout->panels[1].active = false;

  /* Cache splitter ID once */
  g_splitter_id = UI_GenID("LayoutSplitter");
}

void Layout_Update(layout_state *layout, ui_context *ui, rect bounds) {
  /* Animate split ratio towards target */
  f32 diff = layout->target_split_ratio - layout->split_ratio;
  if (diff > 0.001f || diff < -0.001f) {
    layout->split_ratio += diff * ui->dt * ANIMATION_SPEED;
  } else {
    layout->split_ratio = layout->target_split_ratio;
  }

  /* Handle splitter interaction in dual mode */
  if (layout->mode == LAYOUT_MODE_DUAL) {
    /* Focus Switching */
    if (ui->input.key_pressed[KEY_LEFT] && 
       (ui->input.modifiers & MOD_CTRL) && (ui->input.modifiers & MOD_SHIFT)) {
      Layout_SetActivePanel(layout, 0);
    }
    if (ui->input.key_pressed[KEY_RIGHT] && 
       (ui->input.modifiers & MOD_CTRL) && (ui->input.modifiers & MOD_SHIFT)) {
      Layout_SetActivePanel(layout, 1);
    }
    
    f32 available_width = bounds.w;
    f32 split_x = bounds.x + (available_width * layout->split_ratio);

    /* Clamp split position */
    if (split_x < bounds.x + MIN_PANEL_WIDTH)
      split_x = bounds.x + MIN_PANEL_WIDTH;
    if (split_x > bounds.x + bounds.w - MIN_PANEL_WIDTH)
      split_x = bounds.x + bounds.w - MIN_PANEL_WIDTH;

    layout->split_ratio = (split_x - bounds.x) / available_width;
    layout->target_split_ratio = layout->split_ratio;

    rect splitter_bounds = {split_x, bounds.y, SPLITTER_WIDTH, bounds.h};
    bool hover = UI_PointInRect(ui->input.mouse_pos, splitter_bounds);

    /* Start drag */
    if (ui->active == UI_ID_NONE && hover &&
        ui->input.mouse_pressed[MOUSE_LEFT]) {
      ui->active = g_splitter_id;
      layout->dragging = true;
      layout->drag_start_x = (f32)ui->input.mouse_pos.x;
      layout->drag_start_ratio = layout->split_ratio;
    }

    /* Continue drag */
    if (ui->active == g_splitter_id) {
      if (ui->input.mouse_down[MOUSE_LEFT]) {
        f32 dx = (f32)ui->input.mouse_pos.x - layout->drag_start_x;
        f32 d_ratio = dx / available_width;
        layout->target_split_ratio = layout->drag_start_ratio + d_ratio;

        /* Clamp target ratio */
        f32 min_ratio = MIN_PANEL_WIDTH / available_width;
        f32 max_ratio = 1.0f - min_ratio;
        if (layout->target_split_ratio < min_ratio)
          layout->target_split_ratio = min_ratio;
        if (layout->target_split_ratio > max_ratio)
          layout->target_split_ratio = max_ratio;
      } else {
        ui->active = UI_ID_NONE;
        layout->dragging = false;
      }
    }

    /* Handle panel activation (click to focus, but not on splitter) */
    if (!layout->dragging && ui->input.mouse_pressed[MOUSE_LEFT]) {
      rect left_bounds = {bounds.x, bounds.y, split_x - bounds.x, bounds.h};
      rect right_bounds = {split_x + SPLITTER_WIDTH, bounds.y,
                           bounds.w - (split_x - bounds.x) - SPLITTER_WIDTH,
                           bounds.h};

      if (UI_PointInRect(ui->input.mouse_pos, left_bounds)) {
        Layout_SetActivePanel(layout, 0);
      } else if (UI_PointInRect(ui->input.mouse_pos, right_bounds)) {
        Layout_SetActivePanel(layout, 1);
      }
    }
  }

  /* Update the active panel's explorer */
  Explorer_Update(&layout->panels[layout->active_panel_idx].explorer, ui);
}

static void DrawSplitter(ui_context *ui, rect bounds, bool hot, bool active) {
  color c = UI_GetStyleColor(UI_STYLE_BORDER_COLOR);

  if (active) {
    c = UI_GetStyleColor(UI_STYLE_ACTIVE_COLOR);
  } else if (hot) {
    c = UI_GetStyleColor(UI_STYLE_HOVER_COLOR);
  }

  Render_DrawRect(ui->renderer, bounds, c);
}

void Layout_Render(layout_state *layout, ui_context *ui, rect bounds) {
  if (layout->mode == LAYOUT_MODE_SINGLE) {
    Explorer_Render(&layout->panels[0].explorer, ui, bounds, false);
  } else {
    f32 available_width = bounds.w;
    f32 split_x = bounds.x + (available_width * layout->split_ratio);

    rect left_bounds = {bounds.x, bounds.y, split_x - bounds.x, bounds.h};
    rect splitter_bounds = {split_x, bounds.y, SPLITTER_WIDTH, bounds.h};
    rect right_bounds = {split_x + SPLITTER_WIDTH, bounds.y,
                         bounds.w - (split_x - bounds.x) - SPLITTER_WIDTH,
                         bounds.h};

    bool hover = UI_PointInRect(ui->input.mouse_pos, splitter_bounds);

    Explorer_Render(&layout->panels[0].explorer, ui, left_bounds,
                    layout->active_panel_idx == 0);
    DrawSplitter(ui, splitter_bounds, hover, layout->dragging);
    Explorer_Render(&layout->panels[1].explorer, ui, right_bounds,
                    layout->active_panel_idx == 1);
  }
}

void Layout_SetMode(layout_state *layout, layout_mode mode) {
  if (mode == LAYOUT_MODE_DUAL) {
    layout->target_split_ratio = 0.5f;

    /* Clone state from active panel to inactive panel */
    u32 src_idx = layout->active_panel_idx;
    u32 dst_idx = (src_idx + 1) % 2;
    explorer_state *src = &layout->panels[src_idx].explorer;
    explorer_state *dst = &layout->panels[dst_idx].explorer;

    /* Navigate to same path */
    Explorer_NavigateTo(dst, src->fs.current_path);
    
    /* Copy history */
    dst->history_count = src->history_count;
    dst->history_index = src->history_index;
    for(i32 i=0; i<src->history_count; i++) {
        strncpy(dst->history[i], src->history[i], FS_MAX_PATH);
    }
  }

  layout->mode = mode;

  if (mode == LAYOUT_MODE_SINGLE && layout->active_panel_idx == 1) {
    layout->active_panel_idx = 0;
  }
}

void Layout_ToggleMode(layout_state *layout) {
  if (layout->mode == LAYOUT_MODE_SINGLE) {
    Layout_SetMode(layout, LAYOUT_MODE_DUAL);
  } else {
    Layout_SetMode(layout, LAYOUT_MODE_SINGLE);
  }
}

void Layout_SetActivePanel(layout_state *layout, u32 index) {
  if (index > 1)
    return;

  layout->panels[layout->active_panel_idx].active = false;
  layout->active_panel_idx = index;
  layout->panels[layout->active_panel_idx].active = true;
}

panel *Layout_GetActivePanel(layout_state *layout) {
  return &layout->panels[layout->active_panel_idx];
}
