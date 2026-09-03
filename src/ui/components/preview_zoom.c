/*
 * preview_zoom.c - Shared zoom state and indicator for preview content.
 */

#include "preview_zoom.h"
#include "../../core/theme.h"

#include <math.h>
#include <stdio.h>

#define PREVIEW_ZOOM_INDICATOR_WIDTH 184
#define PREVIEW_ZOOM_INDICATOR_HEIGHT 34
#define PREVIEW_ZOOM_TRACK_WIDTH 104
#define PREVIEW_ZOOM_TRACK_HEIGHT 3
#define PREVIEW_ZOOM_THUMB_SIZE 7

static f32 PreviewZoom_ClampScale(f32 scale) {
  return Clamp(scale, PREVIEW_ZOOM_MIN, PREVIEW_ZOOM_MAX);
}

void PreviewZoom_Init(preview_zoom_state *state) {
  if (!state) {
    return;
  }

  state->scale = PREVIEW_ZOOM_DEFAULT;
  state->indicator_until_ms = 0;
}

void PreviewZoom_Reset(preview_zoom_state *state) {
  PreviewZoom_Init(state);
}

b32 PreviewZoom_ApplyWheel(preview_zoom_state *state, f32 wheel_delta,
                           u64 now_ms, f32 *out_old_scale,
                           f32 *out_new_scale) {
  f32 old_scale;
  f32 new_scale;

  if (!state || wheel_delta == 0.0f) {
    return false;
  }

  old_scale = PreviewZoom_ClampScale(state->scale);
  new_scale = PreviewZoom_ClampScale(
      old_scale * powf(PREVIEW_ZOOM_WHEEL_FACTOR, wheel_delta));
  state->scale = new_scale;

  if (out_old_scale) {
    *out_old_scale = old_scale;
  }
  if (out_new_scale) {
    *out_new_scale = new_scale;
  }

  if (new_scale == old_scale) {
    return false;
  }

  state->indicator_until_ms = now_ms + PREVIEW_ZOOM_INDICATOR_MS;
  return true;
}

f32 PreviewZoom_GetSliderPosition(const preview_zoom_state *state) {
  f32 scale = state ? PreviewZoom_ClampScale(state->scale)
                    : PREVIEW_ZOOM_DEFAULT;

  if (scale <= PREVIEW_ZOOM_DEFAULT) {
    return 0.5f *
           (logf(scale / PREVIEW_ZOOM_MIN) /
            logf(PREVIEW_ZOOM_DEFAULT / PREVIEW_ZOOM_MIN));
  }

  return 0.5f +
         0.5f * (logf(scale / PREVIEW_ZOOM_DEFAULT) /
                 logf(PREVIEW_ZOOM_MAX / PREVIEW_ZOOM_DEFAULT));
}

void PreviewZoom_RenderIndicator(const preview_zoom_state *state,
                                 ui_context *ui, rect viewport, u64 now_ms) {
  render_context *ctx;
  const theme *th;
  i64 remaining_ms;
  f32 fade;
  i32 x;
  i32 y;
  rect panel;
  rect track;
  rect fill;
  rect thumb;
  i32 thumb_x;
  char zoom_text[32];
  color panel_color;
  color track_color;
  color fill_color;

  if (!state || !ui || !ui->renderer || state->indicator_until_ms <= now_ms ||
      viewport.w <= PREVIEW_ZOOM_INDICATOR_WIDTH ||
      viewport.h <= PREVIEW_ZOOM_INDICATOR_HEIGHT) {
    return;
  }

  ctx = ui->renderer;
  th = ui->theme;
  remaining_ms = (i64)(state->indicator_until_ms - now_ms);
  fade = remaining_ms < 350 ? (f32)remaining_ms / 350.0f : 1.0f;
  fade = Clamp(fade, 0.0f, 1.0f);

  x = viewport.x + (viewport.w - PREVIEW_ZOOM_INDICATOR_WIDTH) / 2;
  y = viewport.y + viewport.h - PREVIEW_ZOOM_INDICATOR_HEIGHT - 12;
  panel = (rect){x, y, PREVIEW_ZOOM_INDICATOR_WIDTH,
                 PREVIEW_ZOOM_INDICATOR_HEIGHT};

  panel_color = Color_WithAlpha(th->background, (u8)(232.0f * fade));
  track_color = Color_WithAlpha(th->border, (u8)(220.0f * fade));
  fill_color = Color_WithAlpha(th->accent, (u8)(255.0f * fade));

  Render_DrawRectRounded(ctx, panel, th->radius_md, panel_color);

  track = (rect){x + 18, y + 16, PREVIEW_ZOOM_TRACK_WIDTH,
                 PREVIEW_ZOOM_TRACK_HEIGHT};
  Render_DrawRectRounded(ctx, track, 2.0f, track_color);

  thumb_x = track.x + (i32)(PreviewZoom_GetSliderPosition(state) * track.w);
  fill = (rect){track.x, track.y, Max(thumb_x - track.x, 1), track.h};
  Render_DrawRectRounded(ctx, fill, 2.0f, fill_color);

  /* Keep the original-position marker visible above the active track. */
  Render_DrawRect(ctx,
                  (rect){track.x + (track.w / 2), track.y - 4, 1, 11},
                  Color_WithAlpha(th->text_muted, (u8)(190.0f * fade)));

  thumb = (rect){thumb_x - PREVIEW_ZOOM_THUMB_SIZE / 2,
                 track.y - PREVIEW_ZOOM_THUMB_SIZE / 2 + 1,
                 PREVIEW_ZOOM_THUMB_SIZE, PREVIEW_ZOOM_THUMB_SIZE};
  Render_DrawRectRounded(ctx, thumb, 4.0f, fill_color);

  snprintf(zoom_text, sizeof(zoom_text), "%d%%",
           (i32)(PreviewZoom_ClampScale(state->scale) * 100.0f + 0.5f));
  Render_DrawText(ctx, (v2i){x + 136, y + 9}, zoom_text, ui->font,
                  Color_WithAlpha(th->text, (u8)(255.0f * fade)));
}
