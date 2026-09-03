/*
 * preview_zoom.h - Shared zoom state and indicator for preview content.
 *
 * Zoom levels are logarithmic so 100% sits in the middle of the control while
 * still providing useful range at both ends.
 */

#ifndef PREVIEW_ZOOM_H
#define PREVIEW_ZOOM_H

#include "../ui.h"

#define PREVIEW_ZOOM_MIN 0.05f
#define PREVIEW_ZOOM_MAX 20.0f
#define PREVIEW_ZOOM_DEFAULT 1.0f
#define PREVIEW_ZOOM_WHEEL_FACTOR 1.20f
#define PREVIEW_ZOOM_INDICATOR_MS 2200

typedef struct {
  f32 scale;
  u64 indicator_until_ms;
} preview_zoom_state;

void PreviewZoom_Init(preview_zoom_state *state);
void PreviewZoom_Reset(preview_zoom_state *state);

/* Apply wheel input and return the old/new scale when it changed. */
b32 PreviewZoom_ApplyWheel(preview_zoom_state *state, f32 wheel_delta,
                           u64 now_ms, f32 *out_old_scale,
                           f32 *out_new_scale);

/* Map the current scale to the visual slider, where 0.5 is 100%. */
f32 PreviewZoom_GetSliderPosition(const preview_zoom_state *state);

/* Render the temporary, minimalist zoom indicator. */
void PreviewZoom_RenderIndicator(const preview_zoom_state *state,
                                 ui_context *ui, rect viewport, u64 now_ms);

#endif /* PREVIEW_ZOOM_H */
