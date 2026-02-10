/*
 * layout.c - Flexible panel layout system implementation
 */

#include "layout.h"
#include "../config/config.h"
#include "../core/input.h"
#include "components/config_diagnostics.h"
#include "components/dialog.h"

#include <string.h>

#define SPLITTER_WIDTH 4.0f
#define ANIMATION_SPEED 12.0f

/* Cached splitter ID to avoid hashing every frame */
static ui_id g_splitter_id = 0;

void Layout_Init(layout_state *layout, memory_arena *arena) {
  layout->arena = arena;
  layout->mode = WB_LAYOUT_MODE_SINGLE;
  layout->active_panel_idx = 0;
  layout->split_ratio = 0.5f;
  layout->target_split_ratio = 0.5f;
  layout->dragging = false;
  layout->context_menu =
      NULL; /* Will be set by main.c after ContextMenu_Init */
  layout->show_config_diagnostics = false;
  ScrollContainer_Init(&layout->diagnostic_scroll);

  /* Initialize panels */
  Explorer_Init(&layout->panels[0].explorer, arena);
  Explorer_Init(&layout->panels[1].explorer, arena);

  /* Set layout pointer for shared clipboard access */
  layout->panels[0].explorer.layout = layout;
  layout->panels[1].explorer.layout = layout;

  /* Initialize terminal panels (one per split as requested) */
  TerminalPanel_Init(&layout->panels[0].terminal);
  layout->panels[0].terminal.resizer_id = UI_GenID("TerminalResizer0");
  TerminalPanel_Init(&layout->panels[1].terminal);
  layout->panels[1].terminal.resizer_id = UI_GenID("TerminalResizer1");

  layout->panels[0].active = true;
  layout->panels[1].active = false;

  /* Cache splitter ID once */
  g_splitter_id = UI_GenID("LayoutSplitter");

  /* Initialize drag and drop */
  DragDrop_Init(&layout->drag_drop);
}

void Layout_RefreshConfig(layout_state *layout) {
  b32 show_hidden = Config_GetBool("explorer.show_hidden", false);

  for (int i = 0; i < 2; i++) {
    layout->panels[i].explorer.show_hidden = show_hidden;

    /* Config_Poll happens before update/render, so checking directory again
       to handle new file visibility */
    Explorer_Refresh(&layout->panels[i].explorer);
  }
}

void Layout_Update(layout_state *layout, ui_context *ui, rect bounds) {
  /* Update drag and drop system */
  DragDrop_Update(&layout->drag_drop, ui, layout, Platform_GetTimeMs(), ui->dt);

  /* Animate split ratio towards target */
  f32 diff = layout->target_split_ratio - layout->split_ratio;
  if (!g_animations_enabled) {
    layout->split_ratio = layout->target_split_ratio;
  } else if (diff > 0.001f || diff < -0.001f) {
    layout->split_ratio += diff * ui->dt * ANIMATION_SPEED;
  } else {
    layout->split_ratio = layout->target_split_ratio;
  }

  /* Handle splitter interaction in dual mode */
  if (layout->mode == WB_LAYOUT_MODE_DUAL) {
    /* Focus Switching */
    if (ui->input.key_pressed[WB_KEY_LEFT] && (ui->input.modifiers & MOD_CTRL) &&
        (ui->input.modifiers & MOD_SHIFT)) {
      Layout_SetActivePanel(layout, 0);
    }
    if (ui->input.key_pressed[WB_KEY_RIGHT] && (ui->input.modifiers & MOD_CTRL) &&
        (ui->input.modifiers & MOD_SHIFT)) {
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
        ui->input.mouse_pressed[WB_MOUSE_LEFT]) {
      ui->active = g_splitter_id;
      layout->dragging = true;
      layout->drag_start_x = (f32)ui->input.mouse_pos.x;
      layout->drag_start_ratio = layout->split_ratio;
    }

    /* Continue drag */
    if (ui->active == g_splitter_id) {
      if (ui->input.mouse_down[WB_MOUSE_LEFT]) {
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
    if (!layout->dragging && (ui->input.mouse_pressed[WB_MOUSE_LEFT] ||
                              ui->input.mouse_pressed[WB_MOUSE_RIGHT])) {
      rect left_bounds = {bounds.x, bounds.y, split_x - bounds.x, bounds.h};
      rect right_bounds = {split_x + SPLITTER_WIDTH, bounds.y,
                           bounds.w - (split_x - bounds.x) - SPLITTER_WIDTH,
                           bounds.h};

      if (UI_PointInRect(ui->input.mouse_pos, left_bounds)) {
        Layout_SetActivePanel(layout, 0);
        /* Set focus to explorer if click wasn't in terminal area */
        if (!TerminalPanel_IsVisible(&layout->panels[0].terminal) ||
            !UI_PointInRect(ui->input.mouse_pos,
                            layout->panels[0].terminal.last_bounds)) {
          Input_SetFocus(WB_INPUT_TARGET_EXPLORER);
        }
      } else if (UI_PointInRect(ui->input.mouse_pos, right_bounds)) {
        Layout_SetActivePanel(layout, 1);
        if (!TerminalPanel_IsVisible(&layout->panels[1].terminal) ||
            !UI_PointInRect(ui->input.mouse_pos,
                            layout->panels[1].terminal.last_bounds)) {
          Input_SetFocus(WB_INPUT_TARGET_EXPLORER);
        }
      }
    }
  } else {
    /* Single panel mode: handle click-to-focus for explorer */
    if (ui->input.mouse_pressed[WB_MOUSE_LEFT] ||
        ui->input.mouse_pressed[WB_MOUSE_RIGHT]) {
      if (!TerminalPanel_IsVisible(&layout->panels[0].terminal) ||
          !UI_PointInRect(ui->input.mouse_pos,
                          layout->panels[0].terminal.last_bounds)) {
        Input_SetFocus(WB_INPUT_TARGET_EXPLORER);
      }
    }
  }

  /* Poll file watchers for ALL panels (even inactive ones) */
  for (int i = 0; i < 2; i++) {
    Explorer_PollWatcher(&layout->panels[i].explorer);
  }

  /* Update terminal panels FIRST (they have input priority when focused) */
  for (int i = 0; i < 2; i++) {
    TerminalPanel_Update(&layout->panels[i].terminal, ui, ui->dt,
                         (u32)i == layout->active_panel_idx, bounds.h);
  }

  /* Update explorer only if it has focus (keys not consumed by terminal or
   * modal) */
  /* Note: WB_INPUT_TARGET_DIALOG means explorer dialogs (rename, delete), not
   * global modals */
  input_target focus = Input_GetFocus();
  b32 config_modal_open = layout->show_config_diagnostics;
  if (!config_modal_open &&
      (focus == WB_INPUT_TARGET_EXPLORER || focus == WB_INPUT_TARGET_DIALOG ||
       focus == WB_INPUT_TARGET_CONTEXT_MENU)) {
    Explorer_Update(&layout->panels[layout->active_panel_idx].explorer, ui,
                    &layout->drag_drop, layout->active_panel_idx);
  }

  /* Handle global config diagnostic modal */
  if (layout->show_config_diagnostics) {
    if (ui->input.key_pressed[WB_KEY_ESCAPE] ||
        ui->input.key_pressed[WB_KEY_RETURN]) {
      layout->show_config_diagnostics = false;
      UI_EndModal();
      Input_PopFocus();
    }
  }
}

static void DrawSplitter(ui_context *ui, rect bounds, bool hot, bool active) {
  color c = UI_GetStyleColor(WB_UI_STYLE_BORDER_COLOR);

  if (active) {
    c = UI_GetStyleColor(WB_UI_STYLE_ACTIVE_COLOR);
  } else if (hot) {
    c = UI_GetStyleColor(WB_UI_STYLE_HOVER_COLOR);
  }

  Render_DrawRect(ui->renderer, bounds, c);
}

/* Helper to render a single panel with its terminal */
static void RenderPanelWithTerminal(layout_state *layout, ui_context *ui,
                                    rect bounds, u32 panel_idx) {
  panel *p = &layout->panels[panel_idx];
  b32 has_focus = (layout->active_panel_idx == panel_idx);

  /* Calculate terminal height if visible */
  i32 terminal_height = TerminalPanel_GetHeight(&p->terminal, bounds.h);

  /* Adjust explorer bounds to account for terminal */
  rect explorer_bounds = bounds;
  if (terminal_height > 0) {
    explorer_bounds.h = bounds.h - terminal_height;
  }

  /* Render explorer */
  Explorer_Render(&p->explorer, ui, explorer_bounds,
                  has_focus && !TerminalPanel_HasFocus(&p->terminal),
                  &layout->drag_drop, panel_idx);

  /* Render terminal if visible */
  if (terminal_height > 0) {
    TerminalPanel_Render(&p->terminal, ui, bounds);
  }
}

void Layout_Render(layout_state *layout, ui_context *ui, rect bounds) {
  if (layout->mode == WB_LAYOUT_MODE_SINGLE) {
    RenderPanelWithTerminal(layout, ui, bounds, 0);
  } else {
    f32 available_width = bounds.w;
    f32 split_x = bounds.x + (available_width * layout->split_ratio);

    rect left_bounds = {bounds.x, bounds.y, split_x - bounds.x, bounds.h};
    rect splitter_bounds = {split_x, bounds.y, SPLITTER_WIDTH, bounds.h};
    rect right_bounds = {split_x + SPLITTER_WIDTH, bounds.y,
                         bounds.w - (split_x - bounds.x) - SPLITTER_WIDTH,
                         bounds.h};

    bool hover = UI_PointInRect(ui->input.mouse_pos, splitter_bounds);

    RenderPanelWithTerminal(layout, ui, left_bounds, 0);
    DrawSplitter(ui, splitter_bounds, hover, layout->dragging);
    RenderPanelWithTerminal(layout, ui, right_bounds, 1);
  }

  /* Render config diagnostic modal if active */
  ConfigDiagnostics_Render(ui, bounds, layout);

  /* Render drag and drop preview on top of everything */
  DragDrop_RenderPreview(&layout->drag_drop, ui);
}

void Layout_SetMode(layout_state *layout, layout_mode mode) {
  if (mode == WB_LAYOUT_MODE_DUAL) {
    layout->target_split_ratio = 0.5f;

    /* Clone state from active panel to inactive panel */
    u32 src_idx = layout->active_panel_idx;
    u32 dst_idx = (src_idx + 1) % 2;
    explorer_state *src = &layout->panels[src_idx].explorer;
    explorer_state *dst = &layout->panels[dst_idx].explorer;

    /* Navigate to same path */
    Explorer_NavigateTo(dst, src->fs.current_path, false);

    /* Force watcher to watch the correct path (in case NavigateTo returned
     * early) */
    FSWatcher_WatchDirectory(&dst->watcher, src->fs.current_path);

    /* Copy history */
    dst->history_count = src->history_count;
    dst->history_index = src->history_index;
    for (i32 i = 0; i < src->history_count; i++) {
      strncpy(dst->history[i], src->history[i], FS_MAX_PATH);
    }

    /* Focus the new split */
    Layout_SetActivePanel(layout, dst_idx);
  }

  layout->mode = mode;

  if (mode == WB_LAYOUT_MODE_SINGLE && layout->active_panel_idx == 1) {
    layout->active_panel_idx = 0;
  }
}

void Layout_ToggleMode(layout_state *layout) {
  if (layout->mode == WB_LAYOUT_MODE_SINGLE) {
    Layout_SetMode(layout, WB_LAYOUT_MODE_DUAL);
  } else {
    Layout_SetMode(layout, WB_LAYOUT_MODE_SINGLE);
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

void Layout_ToggleTerminal(layout_state *layout) {
  panel *p = Layout_GetActivePanel(layout);
  if (!p)
    return;

  /* Get CWD from explorer */
  const char *cwd = p->explorer.fs.current_path;

  /* Toggle terminal for this panel */
  TerminalPanel_Toggle(&p->terminal, cwd);
}
