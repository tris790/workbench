/*
 * breadcrumb.h - Breadcrumb Navigation Component
 *
 * Displays interactive file path with clickable segments.
 * Supports hover effects, click navigation, and double-click to copy path.
 * C99, handmade hero style.
 */

#ifndef BREADCRUMB_H
#define BREADCRUMB_H

#include "../ui.h"

/* Maximum number of path segments in breadcrumb */
#define BREADCRUMB_MAX_SEGMENTS 32

/* Result from breadcrumb update/render */
typedef struct {
  i32 clicked_segment;  /* Index of clicked segment (-1 if none) */
  b32 double_clicked;   /* True if breadcrumb was double-clicked */
  b32 path_copied;      /* True if path was copied to clipboard */
} breadcrumb_result;

/* Render and update interactive breadcrumb.
 * Handles hover effects, click detection on segments, and double-click to copy.
 *
 * @param ui UI context for rendering and input.
 * @param bounds Rectangle to draw the breadcrumb in.
 * @param path The full path string to display.
 * @return breadcrumb_result with interaction results.
 */
breadcrumb_result Breadcrumb_Render(ui_context *ui, rect bounds, const char *path);

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
