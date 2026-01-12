/*
 * quick_filter.h - Quick Filter Component for File Explorer
 *
 * Provides an inline fuzzy filter that appears when the user starts typing.
 * C99, handmade hero style.
 */

#ifndef QUICK_FILTER_H
#define QUICK_FILTER_H

#include "animation.h"
#include "types.h"
#include "ui.h"

/* ===== Configuration ===== */

#define QUICK_FILTER_MAX_INPUT 128
#define QUICK_FILTER_HEIGHT 32

/* ===== Quick Filter State ===== */

typedef struct {
  /* Filter input */
  char buffer[QUICK_FILTER_MAX_INPUT];
  ui_text_state input_state;

  /* Animation */
  smooth_value fade_anim; /* 0.0 = hidden, 1.0 = visible */

  /* State flags */
  b32 active; /* Filter is currently active (has content) */

  /* Cached bounds for rendering */
  rect bounds;
} quick_filter_state;

/* ===== Quick Filter API ===== */

/* Initialize quick filter state */
void QuickFilter_Init(quick_filter_state *state);

/* Update quick filter - call each frame
 * Returns true if filter consumed input (is active) */
b32 QuickFilter_Update(quick_filter_state *state, ui_context *ui);

/* Render the quick filter overlay at the bottom of the given bounds */
void QuickFilter_Render(quick_filter_state *state, ui_context *ui, rect bounds);

/* Clear the filter and hide the UI */
void QuickFilter_Clear(quick_filter_state *state);

/* Clear the filter buffer but keep the UI active */
void QuickFilter_ClearBuffer(quick_filter_state *state);

/* Set the filter buffer text and force active */
void QuickFilter_SetBuffer(quick_filter_state *state, const char *text);

/* Check if filter is currently active (has content) */
b32 QuickFilter_IsActive(quick_filter_state *state);

/* Get current filter query (empty string if not active) */
const char *QuickFilter_GetQuery(quick_filter_state *state);

#endif /* QUICK_FILTER_H */
