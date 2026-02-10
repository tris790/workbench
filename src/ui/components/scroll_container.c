/*
 * scroll_container.c - Reusable Scroll Container Implementation
 *
 * C99, handmade hero style.
 */

#include "scroll_container.h"
#include "../../core/theme.h"

/* ===== Initialization ===== */

void ScrollContainer_Init(scroll_container_state *state) {
  memset(state, 0, sizeof(*state));

  /* Set smooth scroll speeds */
  state->scroll_v.speed = SCROLL_SMOOTH_SPEED;
  state->scroll_h.speed = SCROLL_SMOOTH_SPEED;
}

/* ===== Update ===== */

b32 ScrollContainer_Update(scroll_container_state *state, ui_context *ui,
                           rect bounds) {
  b32 changed = false;

  /* Store bounds for rendering */
  state->bounds = bounds;
  state->view_size = (v2f){(f32)bounds.w, (f32)bounds.h};

  /* Update smooth scroll animation */
  SmoothValue_Update(&state->scroll_v, ui->dt);
  SmoothValue_Update(&state->scroll_h, ui->dt);

  f32 old_offset = state->offset.y;
  state->offset.y = state->scroll_v.current;
  state->offset.x = state->scroll_h.current;

  /* Calculate max scroll */
  f32 max_scroll = ScrollContainer_GetMaxScroll(state);

  /* Handle scrollbar dragging */
  if (state->is_dragging) {
    /* Set active state to block other interactions */
    ui_id drag_id =
        UI_GenID("ScrollContainer_Drag"); // Use a fixed ID for drag state
    ui->active = drag_id;

    if (ui->input.mouse_down[WB_MOUSE_LEFT]) {
      /* Calculate scroll based on mouse movement */
      f32 track_height =
          state->view_size.y -
          (state->view_size.y / state->content_size.y * state->view_size.y);
      if (track_height < SCROLL_MIN_BAR_HEIGHT) {
        track_height = state->view_size.y - SCROLL_MIN_BAR_HEIGHT;
      }

      if (track_height > 0) {
        f32 mouse_delta = (f32)ui->input.mouse_pos.y - state->drag_start_mouse;
        f32 scroll_delta = (mouse_delta / track_height) * max_scroll;
        state->target_offset.y = state->drag_start_offset + scroll_delta;

        /* Clamp */
        if (state->target_offset.y < 0)
          state->target_offset.y = 0;
        if (state->target_offset.y > max_scroll)
          state->target_offset.y = max_scroll;

        /* Immediate update for drag (no smooth) */
        state->scroll_v.current = state->target_offset.y;
        state->scroll_v.target = state->target_offset.y;
        state->offset.y = state->target_offset.y;
      }
    } else {
      /* Mouse released - stop dragging */
      state->is_dragging = false;
      ui->active = UI_ID_NONE;
    }
  }

  /* Handle mouse wheel when hovering over container */
  if (!state->is_dragging) {
    if (UI_PointInRect(ui->input.mouse_pos, bounds)) {
      if (ui->input.scroll_delta != 0) {
        state->target_offset.y -=
            ui->input.scroll_delta * SCROLL_WHEEL_MULTIPLIER;

        /* Clamp */
        if (state->target_offset.y < 0)
          state->target_offset.y = 0;
        if (state->target_offset.y > max_scroll)
          state->target_offset.y = max_scroll;

        SmoothValue_SetTarget(&state->scroll_v, state->target_offset.y);
      }

      /* Check for scrollbar click to start drag */
      if (ui->input.mouse_pressed[WB_MOUSE_LEFT] &&
          ScrollContainer_NeedsScrollbar(state) && ui->active == UI_ID_NONE) {
        /* Calculate scrollbar bounds */
        f32 ratio = state->view_size.y / state->content_size.y;
        i32 bar_height = (i32)(bounds.h * ratio);
        if (bar_height < SCROLL_MIN_BAR_HEIGHT)
          bar_height = SCROLL_MIN_BAR_HEIGHT;

        f32 scroll_ratio = max_scroll > 0 ? state->offset.y / max_scroll : 0;
        i32 bar_y = bounds.y + (i32)((bounds.h - bar_height) * scroll_ratio);

        rect scrollbar = {bounds.x + bounds.w - SCROLL_SCROLLBAR_OFFSET, bar_y,
                          SCROLL_SCROLLBAR_WIDTH + 4, /* Add hit area padding */
                          bar_height};

        if (UI_PointInRect(ui->input.mouse_pos, scrollbar)) {
          state->is_dragging = true;
          state->drag_start_offset = state->offset.y;
          state->drag_start_mouse = (f32)ui->input.mouse_pos.y;

          /* Set active immediately */
          ui_id drag_id = UI_GenID("ScrollContainer_Drag");
          ui->active = drag_id;
        }
      }
    }
  }

  /* Clamp current offset (for content size changes) */
  if (state->offset.y > max_scroll)
    state->offset.y = max_scroll;
  if (state->offset.y < 0)
    state->offset.y = 0;

  changed = (state->offset.y != old_offset);
  return changed;
}

/* ===== Content Size ===== */

void ScrollContainer_SetContentSize(scroll_container_state *state,
                                    f32 content_height) {
  state->content_size.y = content_height;

  /* Clamp offset if content shrunk */
  f32 max_scroll = ScrollContainer_GetMaxScroll(state);
  if (state->target_offset.y > max_scroll) {
    state->target_offset.y = max_scroll;
    SmoothValue_SetTarget(&state->scroll_v, state->target_offset.y);
  }
}

/* ===== Scrollbar Rendering ===== */

void ScrollContainer_RenderScrollbar(scroll_container_state *state,
                                     ui_context *ui) {
  if (!ScrollContainer_NeedsScrollbar(state))
    return;

  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  rect bounds = state->bounds;
  f32 max_scroll = ScrollContainer_GetMaxScroll(state);

  /* Calculate scrollbar dimensions */
  f32 ratio = state->view_size.y / state->content_size.y;
  i32 bar_height = (i32)(bounds.h * ratio);
  if (bar_height < SCROLL_MIN_BAR_HEIGHT)
    bar_height = SCROLL_MIN_BAR_HEIGHT;

  f32 scroll_ratio = max_scroll > 0 ? state->offset.y / max_scroll : 0;
  i32 bar_y = bounds.y + (i32)((bounds.h - bar_height) * scroll_ratio);

  rect scrollbar = {bounds.x + bounds.w - SCROLL_SCROLLBAR_OFFSET, bar_y,
                    SCROLL_SCROLLBAR_WIDTH, bar_height};

  /* Determine color based on state */
  color bar_color = th->text_muted;

  if (state->is_dragging) {
    bar_color.a = 220;
  } else if (UI_PointInRect(ui->input.mouse_pos, scrollbar)) {
    bar_color.a = 160;
  } else {
    bar_color.a = 100;
  }

  Render_DrawRectRounded(ctx, scrollbar, 3.0f, bar_color);
}

/* ===== Scroll To Position ===== */

void ScrollContainer_ScrollToY(scroll_container_state *state, f32 y,
                               f32 item_height) {
  f32 view_top = state->offset.y;
  f32 view_bottom = view_top + state->view_size.y - item_height;
  f32 max_scroll = ScrollContainer_GetMaxScroll(state);

  /* Only scroll if content exceeds viewport */
  if (max_scroll > 0) {
    if (y < view_top) {
      /* Item is above viewport - scroll up */
      state->target_offset.y = y;
      SmoothValue_SetTarget(&state->scroll_v, state->target_offset.y);
    } else if (y > view_bottom) {
      /* Item is below viewport - scroll down */
      state->target_offset.y = y - state->view_size.y + item_height;
      SmoothValue_SetTarget(&state->scroll_v, state->target_offset.y);
    }
  }
}
