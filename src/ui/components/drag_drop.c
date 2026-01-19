/*
 * drag_drop.c - Drag and Drop Implementation
 */

#include "drag_drop.h"
#include "../../core/input.h"
#include "../../core/theme.h"
#include "../../renderer/icons.h"
#include "../layout.h"
#include "../ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ===== Helper Functions ===== */

/* Calculate distance between two points */
static f32 Distance(v2i a, v2i b) {
  f32 dx = (f32)(a.x - b.x);
  f32 dy = (f32)(a.y - b.y);
  return sqrtf(dx * dx + dy * dy);
}

/* Check if point is inside rectangle */
static b32 PointInRect(v2i point, rect bounds) {
  return point.x >= bounds.x && point.x < bounds.x + bounds.w &&
         point.y >= bounds.y && point.y < bounds.y + bounds.h;
}

/* Check if path A is ancestor of path B (includes self) */
static b32 IsAncestorPath(const char *ancestor, const char *descendant) {
  usize len = strlen(ancestor);
  if (len == 0)
    return 0;

  /* Compare prefix */
  if (strncmp(ancestor, descendant, len) != 0)
    return 0;

  /* Must be exact match or followed by separator */
  if (descendant[len] == '\0')
    return 1; /* Same path */
  if (descendant[len] == '/' || descendant[len] == '\\')
    return 1;

  return 0;
}

/* ===== Lifecycle API ===== */

void DragDrop_Init(drag_drop_state *state) {
  memset(state, 0, sizeof(*state));
  state->state = DRAG_STATE_IDLE;
  state->item_count = 0;
  state->target_type = DROP_TARGET_NONE;

  /* Initialize animation values */
  state->pickup_anim.current = 0.0f;
  state->pickup_anim.target = 0.0f;
  state->pickup_anim.speed = 15.0f; /* Fast pickup */

  state->hover_glow.current = 0.15f;
  state->hover_glow.target = 0.15f;
  state->hover_glow.speed = 3.0f; /* Slow pulse */

  state->drop_anim.current = 0.0f;
  state->drop_anim.target = 0.0f;
  state->drop_anim.speed = 8.0f; /* Medium drop */

  state->preview_offset.x = DRAG_PREVIEW_OFFSET_X;
  state->preview_offset.y = DRAG_PREVIEW_OFFSET_Y;
}

/* ===== Drag Initiation ===== */

void DragDrop_BeginPotential(drag_drop_state *state, fs_state *fs,
                             i32 clicked_idx, u32 panel_idx, v2i mouse_pos,
                             u64 time_ms) {
  /* Don't start drag on ".." entry */
  fs_entry *clicked_entry = FS_GetEntry(fs, clicked_idx);
  if (!clicked_entry || strcmp(clicked_entry->name, "..") == 0) {
    return;
  }

  state->state = DRAG_STATE_PENDING;
  state->start_mouse_pos = mouse_pos;
  state->current_mouse_pos = mouse_pos;
  state->start_time_ms = time_ms;
  state->source_panel_idx = panel_idx;
  state->item_count = 0;
  state->primary_index = 0;

  /* If clicked item is not selected, select only it */
  if (!FS_IsSelected(fs, clicked_idx)) {
    FS_SelectSingle(fs, clicked_idx);
  }

  /* Collect all selected items */
  i32 idx = FS_GetFirstSelected(fs);
  while (idx >= 0 && state->item_count < DRAG_MAX_ITEMS) {
    fs_entry *entry = FS_GetEntry(fs, idx);
    if (entry && strcmp(entry->name, "..") != 0) {
      drag_item *item = &state->items[state->item_count];
      strncpy(item->path, entry->path, FS_MAX_PATH - 1);
      item->path[FS_MAX_PATH - 1] = '\0';
      strncpy(item->name, entry->name, FS_MAX_NAME - 1);
      item->name[FS_MAX_NAME - 1] = '\0';
      item->icon = entry->icon;
      item->is_directory = entry->is_directory;
      item->size = entry->size;

      /* Track which one was clicked */
      if (idx == clicked_idx) {
        state->primary_index = state->item_count;
      }

      state->item_count++;
    }
    idx = FS_GetNextSelected(fs, idx);
  }

  /* Reset target */
  state->target_type = DROP_TARGET_NONE;
  state->target_path[0] = '\0';
}

/* ===== Update ===== */

void DragDrop_Update(drag_drop_state *state, ui_context *ui,
                     layout_state *layout, u64 time_ms, f32 dt) {
  ui_input *input = &ui->input;

  /* Update animations */
  SmoothValue_Update(&state->pickup_anim, dt);
  SmoothValue_Update(&state->hover_glow, dt);
  SmoothValue_Update(&state->drop_anim, dt);

  /* State machine */
  switch (state->state) {
  case DRAG_STATE_IDLE:
    /* Nothing to do */
    break;

  case DRAG_STATE_PENDING: {
    state->current_mouse_pos = input->mouse_pos;

    /* Check if mouse released before threshold */
    if (!input->mouse_down[MOUSE_LEFT]) {
      state->state = DRAG_STATE_IDLE;
      break;
    }

    /* Check distance threshold */
    f32 dist = Distance(state->start_mouse_pos, state->current_mouse_pos);

    if (dist >= DRAG_THRESHOLD_DISTANCE) {
      /* Transition to dragging */
      state->state = DRAG_STATE_DRAGGING;
      SmoothValue_SetTarget(&state->pickup_anim, 1.0f);
    }
    break;
  }

  case DRAG_STATE_DRAGGING: {
    state->current_mouse_pos = input->mouse_pos;

    /* Check for window focus loss */
    if (!ui->window_focused) {
      DragDrop_Cancel(state);
      break;
    }

    /* Check for cancel (Escape) */
    if (input->key_pressed[KEY_ESCAPE]) {
      DragDrop_Cancel(state);
      break;
    }

    /* Check for drop (mouse release) */
    /* Note: We use target_type from previous frame's render pass */
    if (!input->mouse_down[MOUSE_LEFT]) {
      if (state->target_type == DROP_TARGET_FOLDER ||
          state->target_type == DROP_TARGET_PANEL) {
        /* Valid drop - execute move */
        state->state = DRAG_STATE_DROPPING;
        SmoothValue_SetTarget(&state->drop_anim, 1.0f);
      } else {
        /* Invalid or no target - cancel */
        DragDrop_Cancel(state);
      }
      break;
    }

    /* Reset target for current frame - will be re-populated by CheckTarget
     * calls during Render pass */
    state->target_type = DROP_TARGET_NONE;
    break;
  }

  case DRAG_STATE_DROPPING: {
    /* Wait for drop animation */
    if (state->drop_anim.current >= 0.95f) {
      /* Execute the file move operations */
      for (i32 i = 0; i < state->item_count; i++) {
        char dest_path[FS_MAX_PATH];
        FS_JoinPath(dest_path, FS_MAX_PATH, state->target_path,
                    state->items[i].name);

        /* Check if destination already exists */
        if (FS_Exists(dest_path)) {
          fprintf(stderr, "DragDrop: Skip move, destination exists: %s\n",
                  dest_path);
          continue;
        }

        /* Perform the move */
        if (!FS_Rename(state->items[i].path, dest_path)) {
          fprintf(stderr, "DragDrop: Failed to move: %s -> %s\n",
                  state->items[i].path, dest_path);
        }
      }

      /* Refresh both panels */
      Explorer_Refresh(&layout->panels[0].explorer);
      if (layout->mode == LAYOUT_MODE_DUAL) {
        Explorer_Refresh(&layout->panels[1].explorer);
      }

      /* Reset to idle */
      DragDrop_Init(state);
      Platform_SetCursor(CURSOR_DEFAULT);
    }
    break;
  }
  }

  /* Update hover glow pulse when over valid target */
  if (state->state == DRAG_STATE_DRAGGING) {
    if (state->target_type == DROP_TARGET_FOLDER ||
        state->target_type == DROP_TARGET_PANEL) {
      /* Oscillate between 0.15 and 0.25 */
      f32 pulse = 0.20f + 0.05f * sinf((f32)time_ms * 0.003f);
      state->hover_glow.current = pulse;
      Platform_SetCursor(CURSOR_GRABBING);
    } else if (state->target_type == DROP_TARGET_INVALID) {
      state->hover_glow.current = 0.0f;
      Platform_SetCursor(CURSOR_NO_DROP);
    } else {
      state->hover_glow.current = 0.0f;
      Platform_SetCursor(CURSOR_GRABBING);
    }
  }
}

/* ===== Drop Target Detection ===== */

void DragDrop_CheckTarget(drag_drop_state *state, const fs_entry *entry,
                          rect item_bounds, u32 panel_idx) {
  if (state->state != DRAG_STATE_DRAGGING)
    return;
  if (!entry->is_directory)
    return;

  /* Check if mouse is over this item */
  if (!PointInRect(state->current_mouse_pos, item_bounds))
    return;

  /* Use resolved path for comparisons (especially for "..") */
  char resolved_path[FS_MAX_PATH];
  const char *target_path = entry->path;
  if (strcmp(entry->name, "..") == 0) {
    if (FS_ResolvePath(entry->path, resolved_path, FS_MAX_PATH)) {
      target_path = resolved_path;
    }
  }

  /* Check if target is the same directory as source */
  if (state->item_count > 0) {
    char source_dir[FS_MAX_PATH];
    strncpy(source_dir, state->items[0].path, FS_MAX_PATH - 1);
    char *last_sep = strrchr(source_dir, '/');
    if (!last_sep)
      last_sep = strrchr(source_dir, '\\');
    if (last_sep)
      *last_sep = '\0';

    if (FS_PathsEqual(source_dir, target_path)) {
      return; /* Silent no-op for same location */
    }
  }

  /* Check for invalid targets (folder into self or subfolder) */
  if (DragDrop_IsInvalidTarget(state, target_path)) {
    state->target_type = DROP_TARGET_INVALID;
    state->target_bounds = item_bounds;
    state->target_panel_idx = panel_idx;
    strncpy(state->target_path, target_path, FS_MAX_PATH - 1);
    return;
  }

  /* Valid folder target */
  state->target_type = DROP_TARGET_FOLDER;
  state->target_bounds = item_bounds;
  state->target_panel_idx = panel_idx;
  strncpy(state->target_path, target_path, FS_MAX_PATH - 1);
  state->target_path[FS_MAX_PATH - 1] = '\0';
}

void DragDrop_CheckPanelTarget(drag_drop_state *state, const char *panel_path,
                               rect panel_bounds, u32 panel_idx) {
  if (state->state != DRAG_STATE_DRAGGING)
    return;

  /* Only set panel as target if no folder target is set */
  if (state->target_type == DROP_TARGET_FOLDER)
    return;

  /* Check if mouse is in panel bounds */
  if (!PointInRect(state->current_mouse_pos, panel_bounds))
    return;

  /* Check for invalid (same directory as source) */
  /* Get source directory */
  if (state->item_count > 0) {
    char source_dir[FS_MAX_PATH];
    strncpy(source_dir, state->items[0].path, FS_MAX_PATH - 1);

    /* Find last separator and truncate */
    char *last_sep = strrchr(source_dir, '/');
    if (!last_sep)
      last_sep = strrchr(source_dir, '\\');
    if (last_sep)
      *last_sep = '\0';

    /* If dropping into same directory, it's effectively a no-op */
    if (FS_PathsEqual(source_dir, panel_path)) {
      return; /* Don't show as target */
    }
  }

  /* Valid panel target */
  state->target_type = DROP_TARGET_PANEL;
  state->target_bounds = panel_bounds;
  state->target_panel_idx = panel_idx;
  strncpy(state->target_path, panel_path, FS_MAX_PATH - 1);
  state->target_path[FS_MAX_PATH - 1] = '\0';
}

/* ===== Control ===== */

void DragDrop_Cancel(drag_drop_state *state) {
  state->state = DRAG_STATE_IDLE;
  state->item_count = 0;
  state->target_type = DROP_TARGET_NONE;
  SmoothValue_SetImmediate(&state->pickup_anim, 0.0f);
  SmoothValue_SetImmediate(&state->drop_anim, 0.0f);
  Platform_SetCursor(CURSOR_DEFAULT);
}

/* ===== Query Functions ===== */

b32 DragDrop_IsActive(drag_drop_state *state) {
  return state->state != DRAG_STATE_IDLE;
}

b32 DragDrop_IsDragging(drag_drop_state *state) {
  return state->state == DRAG_STATE_DRAGGING;
}

u32 DragDrop_GetSourcePanel(drag_drop_state *state) {
  return state->source_panel_idx;
}

b32 DragDrop_IsMouseOver(drag_drop_state *state, rect bounds) {
  return PointInRect(state->current_mouse_pos, bounds);
}

b32 DragDrop_IsInvalidTarget(drag_drop_state *state, const char *target_path) {
  /* Check each dragged item */
  for (i32 i = 0; i < state->item_count; i++) {
    /* Cannot drop folder into itself or descendant */
    if (state->items[i].is_directory) {
      if (IsAncestorPath(state->items[i].path, target_path)) {
        return 1;
      }
    }
  }
  return 0;
}

/* ===== Rendering Helper ===== */

static void DrawRectOutline(ui_context *ui, rect r, color c, i32 thickness) {
  Render_DrawRect(ui->renderer, (rect){r.x, r.y, r.w, thickness}, c); /* Top */
  Render_DrawRect(ui->renderer,
                  (rect){r.x, r.y + r.h - thickness, r.w, thickness},
                  c); /* Bottom */
  Render_DrawRect(ui->renderer,
                  (rect){r.x, r.y + thickness, thickness, r.h - thickness * 2},
                  c); /* Left */
  Render_DrawRect(ui->renderer,
                  (rect){r.x + r.w - thickness, r.y + thickness, thickness,
                         r.h - thickness * 2},
                  c); /* Right */
}

/* ===== Rendering ===== */

#define PREVIEW_WIDTH 200
#define PREVIEW_HEIGHT 48
#define PREVIEW_ICON_SIZE 24
#define PREVIEW_PADDING 8
#define PREVIEW_STACK_OFFSET 3
#define PREVIEW_OPACITY 0.75f

void DragDrop_RenderPreview(drag_drop_state *state, ui_context *ui) {
  if (state->state != DRAG_STATE_DRAGGING &&
      state->state != DRAG_STATE_DROPPING)
    return;
  if (state->item_count == 0)
    return;

  f32 scale = 1.0f + 0.08f * state->pickup_anim.current;
  i32 base_x = state->current_mouse_pos.x + state->preview_offset.x;
  i32 base_y = state->current_mouse_pos.y + state->preview_offset.y;

  /* Drop animation: move toward target */
  if (state->state == DRAG_STATE_DROPPING) {
    f32 t = state->drop_anim.current;
    if (t > 1.0f)
      t = 1.0f;
    /* Quadratic easing for drop */
    t = t * t;

    i32 target_x = state->target_bounds.x + state->target_bounds.w / 2 -
                   (i32)(PREVIEW_WIDTH / 2);
    i32 target_y = state->target_bounds.y + state->target_bounds.h / 2 -
                   (i32)(PREVIEW_HEIGHT / 2);

    base_x = (i32)((1.0f - t) * (f32)base_x + t * (f32)target_x);
    base_y = (i32)((1.0f - t) * (f32)base_y + t * (f32)target_y);
  }

  i32 stack_count = state->item_count;
  if (stack_count > 3)
    stack_count = 3;

  /* Render cards back-to-front */
  for (i32 i = stack_count - 1; i >= 0; i--) {
    i32 offset = i * PREVIEW_STACK_OFFSET;
    rect bounds = {base_x + offset, base_y + offset,
                   (i32)(PREVIEW_WIDTH * scale), (i32)(PREVIEW_HEIGHT * scale)};
    f32 opacity = PREVIEW_OPACITY - i * 0.1f;
    if (opacity < 0.2f)
      opacity = 0.2f;

    /* Shadow */
    color shadow = {0, 0, 0, (u8)(opacity * 100)};
    Render_DrawRect(ui->renderer,
                    (rect){bounds.x + 4, bounds.y + 4, bounds.w, bounds.h},
                    shadow);

    /* Card bg */
    color bg = ui->theme->panel;
    bg.a = (u8)(opacity * 255);
    Render_DrawRectRounded(ui->renderer, bounds, 6.0f, bg);

    /* Border */
    color border = ui->theme->accent;
    border.a = (u8)(opacity * 255);
    DrawRectOutline(ui, bounds, border, 1);

    /* Top card: show icon + text */
    if (i == 0) {
      drag_item *item = &state->items[state->primary_index];

      /* Icon */
      i32 icon_size = (i32)(PREVIEW_ICON_SIZE * scale);
      rect icon_bounds = {bounds.x + PREVIEW_PADDING,
                          bounds.y + (bounds.h - icon_size) / 2, icon_size,
                          icon_size};

      Icon_Draw(ui->renderer, icon_bounds, item->icon,
                Color_WithAlpha(ui->theme->text, (u8)(opacity * 255)));

      /* Name */
      char display_name[300];
      char truncated_name[32];

      const char *name_to_use = item->name;
      if (strlen(item->name) > 25) {
        strncpy(truncated_name, item->name, 22);
        truncated_name[22] = '\0';
        strcat(truncated_name, "...");
        name_to_use = truncated_name;
      }

      if (state->item_count > 1) {
        snprintf(display_name, sizeof(display_name), "%s (+%d)", name_to_use,
                 state->item_count - 1);
      } else {
        strncpy(display_name, name_to_use, sizeof(display_name) - 1);
        display_name[sizeof(display_name) - 1] = '\0';
      }

      v2i text_pos = {icon_bounds.x + icon_size + PREVIEW_PADDING,
                      bounds.y + (bounds.h - Font_GetLineHeight(ui->main_font)) / 2};

      Render_DrawText(ui->renderer, text_pos, display_name, ui->main_font,
                      Color_WithAlpha(ui->theme->text, (u8)(opacity * 255)));
    }
  }
}

void DragDrop_RenderTargetHighlight(drag_drop_state *state, ui_context *ui,
                                    rect bounds) {
  if (state->state != DRAG_STATE_DRAGGING)
    return;

  if (state->target_type == DROP_TARGET_INVALID) {
    color tint = Color_WithAlpha(ui->theme->error, 40);
    Render_DrawRect(ui->renderer, bounds, tint);
    DrawRectOutline(ui, bounds, ui->theme->error, 2);
  } else if (state->target_type == DROP_TARGET_FOLDER ||
             state->target_type == DROP_TARGET_PANEL) {
    color bg = Color_WithAlpha(ui->theme->accent,
                               (u8)(state->hover_glow.current * 255.0f));
    Render_DrawRectRounded(ui->renderer, bounds, 4.0f, bg);
    DrawRectOutline(ui, bounds, ui->theme->accent, 2);
  }
}
