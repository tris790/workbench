/*
 * scroll_container.h - Reusable Scroll Container Component
 *
 * Provides a unified scroll container with:
 * - Mouse wheel scrolling
 * - Scrollbar rendering and dragging
 * - Smooth scroll animations
 * - Scroll-to-item support
 *
 * C99, handmade hero style.
 */

#ifndef SCROLL_CONTAINER_H
#define SCROLL_CONTAINER_H

#include "../../core/animation.h"
#include "../../core/types.h"
#include "../ui.h"

/* ===== Configuration ===== */

#define SCROLL_SCROLLBAR_WIDTH 6
#define SCROLL_SCROLLBAR_GUTTER 12
#define SCROLL_SCROLLBAR_OFFSET 8
#define SCROLL_MIN_BAR_HEIGHT 20
#define SCROLL_WHEEL_MULTIPLIER 80.0f
#define SCROLL_SMOOTH_SPEED 1500.0f

/* ===== Scroll Container State ===== */

typedef struct {
  /* Scroll position */
  v2f offset;        /* Current scroll position */
  v2f target_offset; /* Target scroll position (for smooth scrolling) */

  /* Size tracking */
  v2f content_size; /* Total content size */
  v2f view_size;    /* Visible viewport size */

  /* Smooth animation */
  smooth_value scroll_v; /* Vertical smooth scroll */
  smooth_value scroll_h; /* Horizontal smooth scroll */

  /* Scrollbar drag state */
  b32 is_dragging;       /* Scrollbar drag in progress */
  f32 drag_start_offset; /* Scroll offset when drag started */
  f32 drag_start_mouse;  /* Mouse Y when drag started */

  /* Bounds cache for input handling */
  rect bounds; /* Container bounds */
} scroll_container_state;

/* ===== API ===== */

/* Initialize scroll container with default settings */
void ScrollContainer_Init(scroll_container_state *state);

/* Update scroll state - handles input (wheel, drag), animation
 * Call this at the start of your scroll region
 * Returns true if scroll position changed */
b32 ScrollContainer_Update(scroll_container_state *state, ui_context *ui,
                           rect bounds);

/* Set content size - call this after measuring content */
void ScrollContainer_SetContentSize(scroll_container_state *state,
                                    f32 content_height);

/* Render scrollbar - call after content is rendered and clip is reset */
void ScrollContainer_RenderScrollbar(scroll_container_state *state,
                                     ui_context *ui);

/* Utility: scroll to make a Y position visible within the viewport */
void ScrollContainer_ScrollToY(scroll_container_state *state, f32 y,
                               f32 item_height);

/* Get current scroll offset */
static inline f32 ScrollContainer_GetOffsetY(scroll_container_state *state) {
  return state->offset.y;
}

/* Get maximum scroll value */
static inline f32 ScrollContainer_GetMaxScroll(scroll_container_state *state) {
  f32 max = state->content_size.y - state->view_size.y;
  return max > 0 ? max : 0;
}

/* Check if scrollbar is needed */
static inline b32
ScrollContainer_NeedsScrollbar(scroll_container_state *state) {
  return state->content_size.y > state->view_size.y;
}

#endif /* SCROLL_CONTAINER_H */
