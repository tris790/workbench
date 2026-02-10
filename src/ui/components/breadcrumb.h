/*
 * breadcrumb.h - Breadcrumb Navigation Component
 *
 * Displays interactive file path with clickable segments.
 * Supports hover effects, click navigation, and click-to-edit mode.
 * C99, handmade hero style.
 */

#ifndef BREADCRUMB_H
#define BREADCRUMB_H

#include "../ui.h"

/* Maximum number of path segments in breadcrumb */
#define BREADCRUMB_MAX_SEGMENTS 32
#define BREADCRUMB_MAX_PATH FS_MAX_PATH

/* Breadcrumb state for editable path */
typedef struct {
  b32 is_editing;       /* True when in edit mode */
  ui_text_state text_state; /* Text input state for editing */
  char edit_buffer[BREADCRUMB_MAX_PATH]; /* Buffer for edited path */

  /* Click detection state (per-instance to avoid sharing between breadcrumbs) */
  u64 last_click_time;  /* For double-click detection */
  b32 was_hovered;      /* Previous frame hover state */
  u64 copy_feedback_time; /* When path was copied to clipboard */
} breadcrumb_state;

/* Result from breadcrumb update/render */
typedef struct {
  i32 clicked_segment;  /* Index of clicked segment (-1 if none) */
  b32 path_copied;      /* True if path was copied to clipboard */
  b32 editing_started;  /* True if user clicked to start editing */
  b32 editing_finished; /* True if user pressed Enter to confirm edit */
  b32 editing_cancelled; /* True if user pressed Escape to cancel edit */
  b32 text_changed;     /* True if text was modified (typed/pasted) */
} breadcrumb_result;

/* Initialize breadcrumb state */
void Breadcrumb_Init(breadcrumb_state *state);

/* Start editing mode with current path */
void Breadcrumb_StartEditing(breadcrumb_state *state, const char *path);

/* Cancel editing mode */
void Breadcrumb_CancelEditing(breadcrumb_state *state);

/* Render and update interactive breadcrumb.
 * Handles hover effects, click detection on segments, and click-to-edit.
 *
 * @param ui UI context for rendering and input.
 * @param bounds Rectangle to draw the breadcrumb in.
 * @param path The full path string to display.
 * @param state Breadcrumb state for editing mode.
 * @return breadcrumb_result with interaction results.
 */
breadcrumb_result Breadcrumb_Render(ui_context *ui, rect bounds, const char *path,
                                     breadcrumb_state *state);

/* Get the path up to and including the specified segment.
 * Used to navigate when a segment is clicked.
 *
 * @param path The full path.
 * @param segment_index The segment index to navigate to.
 * @param out_buffer Output buffer for the resulting path.
 * @param buffer_size Size of the output buffer.
 * @return true if successful.
 */
b32 Breadcrumb_GetPathForSegment(const char *path, i32 segment_index,
                                  char *out_buffer, usize buffer_size);

#endif /* BREADCRUMB_H */
