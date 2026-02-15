/*
 * progress_bar.h - Progress Bar UI Component for Workbench
 *
 * Displays progress of background tasks.
 * Supports bounded (known progress) and unbounded (indeterminate) modes.
 * C99, handmade hero style.
 */

#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include "../../core/task_queue.h"
#include "../ui.h"

/* ===== Configuration ===== */

#define PROGRESS_BAR_HEIGHT 3       /* Thin bar height in pixels */
#define PROGRESS_BAR_SHOW_DELAY_MS 1000  /* Delay before showing (1 second) */
#define PROGRESS_BAR_ANIMATION_SPEED 600.0f  /* Pixels per second for unbounded mode */

/* ===== Progress Bar State ===== */

typedef struct {
  /* Animation state for unbounded mode */
  f32 unbounded_offset;  /* Current position of leading edge */
  f32 segment_width;     /* Width of the moving segment in unbounded mode */
  
  /* Visibility animation */
  smooth_value visibility;  /* 0.0 = hidden, 1.0 = fully visible */
  b32 should_show;          /* Whether bar should be visible */
  
  /* Bounded progress smoothing - exponential decay for fluid motion */
  f32 current_progress;   /* Smoothed progress value (0.0 - 1.0) */
  f32 target_progress;    /* Target progress from task */
  f32 smooth_factor;      /* Blend factor per second (0.0 - 1.0, higher = faster) */
  
  /* Config */
  f32 show_delay_ms;
  color bar_color;
} progress_bar_state;

/* ===== Progress Bar API ===== */

/* Initialize progress bar state with default values. */
void ProgressBar_Init(progress_bar_state *bar);

/* Initialize with custom color. */
void ProgressBar_InitWithColor(progress_bar_state *bar, color bar_color);

/* Update progress bar (call each frame).
 * is_busy: Whether there's work in progress
 * elapsed_ms: How long the current task has been running
 * progress: Current task progress (NULL if no task) */
void ProgressBar_Update(progress_bar_state *bar, b32 is_busy, u64 elapsed_ms, 
                        const task_progress *progress, f32 dt);

/* Render progress bar at the bottom of the given bounds.
 * Returns the height of the rendered bar (0 if not visible). */
f32 ProgressBar_Render(progress_bar_state *bar, ui_context *ui, rect bounds, 
                       const task_progress *progress);

/* Check if progress bar is currently visible (or animating in/out). */
b32 ProgressBar_IsVisible(progress_bar_state *bar);

#endif /* PROGRESS_BAR_H */
