/*
 * progress_bar.c - Progress Bar UI Component Implementation
 *
 * C99, handmade hero style.
 */

#include "progress_bar.h"
#include "../../core/animation.h"
#include "../../core/theme.h"

#include <math.h>

/* ===== Configuration ===== */

#define UNBOUNDED_SEGMENT_RATIO 0.3f  /* Segment is 30% of bar width */

/* ===== Public API ===== */

void ProgressBar_Init(progress_bar_state *bar) {
  const theme *th = Theme_GetCurrent();
  ProgressBar_InitWithColor(bar, th->accent);
}

void ProgressBar_InitWithColor(progress_bar_state *bar, color bar_color) {
  memset(bar, 0, sizeof(*bar));
  bar->bar_color = bar_color;
  bar->show_delay_ms = PROGRESS_BAR_SHOW_DELAY_MS;
  bar->visibility.current = 0.0f;
  bar->visibility.target = 0.0f;
  bar->visibility.speed = 8.0f;  /* Fade in/out speed */
  bar->should_show = false;
  bar->unbounded_offset = 0.0f;
  bar->segment_width = 100.0f;  /* Initial segment width, updated based on bounds */
  bar->current_progress = 0.0f;
  bar->target_progress = 0.0f;
  bar->smooth_factor = 8.0f;  /* Higher = more responsive but still smooth */
}

void ProgressBar_Update(progress_bar_state *bar, b32 is_busy, u64 elapsed_ms, 
                        const task_progress *progress, f32 dt) {
  /* Determine if bar should be shown */
  bar->should_show = is_busy && (elapsed_ms >= bar->show_delay_ms);
  
  /* Update visibility target */
  SmoothValue_SetTarget(&bar->visibility, bar->should_show ? 1.0f : 0.0f);
  
  /* Update smooth value */
  SmoothValue_Update(&bar->visibility, dt);
  
  /* Update bounded progress target if we have progress info */
  if (progress && progress->type == WB_PROGRESS_TYPE_BOUNDED) {
    if (progress->data.bounded.total > 0) {
      bar->target_progress = (f32)progress->data.bounded.current / (f32)progress->data.bounded.total;
      if (bar->target_progress > 1.0f) bar->target_progress = 1.0f;
      if (bar->target_progress < 0.0f) bar->target_progress = 0.0f;
    }
  } else if (!is_busy) {
    /* Reset progress when idle */
    bar->target_progress = 0.0f;
  }
  
  /* Smooth progress using exponential decay: current += (target - current) * factor * dt
   * This creates fluid, organic motion that never overshoots */
  f32 delta = bar->target_progress - bar->current_progress;
  if (delta != 0.0f) {
    f32 move = delta * bar->smooth_factor * dt;
    /* Prevent micro-jitter when very close */
    if (delta > 0 && move < 0.0001f && delta > 0.0001f) move = 0.0001f;
    if (delta < 0 && move > -0.0001f && delta < -0.0001f) move = -0.0001f;
    
    bar->current_progress += move;
    /* Snap to target when very close */
    if ((delta > 0 && bar->current_progress > bar->target_progress) ||
        (delta < 0 && bar->current_progress < bar->target_progress)) {
      bar->current_progress = bar->target_progress;
    }
  }
  
  /* Update unbounded animation if visible and in unbounded mode */
  if (bar->visibility.current > 0.01f && is_busy) {
    bar->unbounded_offset += PROGRESS_BAR_ANIMATION_SPEED * dt;
  }
}

f32 ProgressBar_Render(progress_bar_state *bar, ui_context *ui, rect bounds,
                       const task_progress *progress) {
  (void)progress;
  
  if (bar->visibility.current < 0.01f) {
    return 0.0f;
  }
  
  render_context *ctx = ui->renderer;
  
  /* Calculate bar bounds at bottom of given bounds */
  f32 bar_height = PROGRESS_BAR_HEIGHT * bar->visibility.current;
  rect bar_bounds = {
    bounds.x,
    bounds.y + bounds.h - bar_height,
    bounds.w,
    bar_height
  };
  
  /* Apply alpha based on visibility */
  color draw_color = bar->bar_color;
  draw_color.a = (u8)(255.0f * bar->visibility.current);
  
  if (progress == NULL) {
    /* No progress info - show simple indeterminate animation */
    /* Loop a segment across the bar */
    f32 segment_width = bounds.w * UNBOUNDED_SEGMENT_RATIO;
    f32 total_width = bounds.w + segment_width * 2.0f;
    f32 offset = fmodf(bar->unbounded_offset, total_width) - segment_width;
    
    rect segment = {
      bounds.x + offset,
      bar_bounds.y,
      segment_width,
      bar_height
    };
    
    /* Clamp to bar bounds */
    if (segment.x < bounds.x) {
      segment.w -= (bounds.x - segment.x);
      segment.x = bounds.x;
    }
    if (segment.x + segment.w > bounds.x + bounds.w) {
      segment.w = (bounds.x + bounds.w) - segment.x;
    }
    
    if (segment.w > 0) {
      Render_DrawRect(ctx, segment, draw_color);
    }
  } else if (progress->type == WB_PROGRESS_TYPE_BOUNDED) {
    /* Bounded progress - fill bar proportionally using smoothed value */
    f32 ratio = bar->current_progress;
    
    rect filled = {
      bar_bounds.x,
      bar_bounds.y,
      bar_bounds.w * ratio,
      bar_height
    };
    
    Render_DrawRect(ctx, filled, draw_color);
  } else {
    /* Unbounded progress - looping segment animation */
    f32 segment_width = bounds.w * UNBOUNDED_SEGMENT_RATIO;
    f32 total_width = bounds.w + segment_width * 2.0f;
    f32 offset = fmodf(bar->unbounded_offset, total_width) - segment_width;
    
    rect segment = {
      bounds.x + offset,
      bar_bounds.y,
      segment_width,
      bar_height
    };
    
    /* Clamp to bar bounds */
    if (segment.x < bounds.x) {
      segment.w -= (bounds.x - segment.x);
      segment.x = bounds.x;
    }
    if (segment.x + segment.w > bounds.x + bounds.w) {
      segment.w = (bounds.x + bounds.w) - segment.x;
    }
    
    if (segment.w > 0) {
      Render_DrawRect(ctx, segment, draw_color);
    }
  }
  
  return PROGRESS_BAR_HEIGHT;
}

b32 ProgressBar_IsVisible(progress_bar_state *bar) {
  return bar->visibility.current > 0.01f || bar->should_show;
}
