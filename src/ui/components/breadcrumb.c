/*
 * breadcrumb.c - Breadcrumb Navigation Component Implementation
 *
 * Interactive breadcrumb with clickable segments and double-click to copy.
 * C99, handmade hero style.
 */

#include "breadcrumb.h"
#include "fs.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

/* ===== Configuration ===== */

#define BREADCRUMB_PADDING 8
#define BREADCRUMB_SEGMENT_SPACING 4
#define BREADCRUMB_DOUBLE_CLICK_MS 400
#define BREADCRUMB_SEPARATOR "/"

/* Static state for double-click detection */
static u64 s_last_click_time = 0;
static b32 s_was_hovered = false;

/* Copy feedback state */
static u64 s_copy_feedback_time = 0;
#define BREADCRUMB_COPY_FEEDBACK_MS 1200

/* ===== Helper Functions ===== */

/* Count segments in path and store their start positions */
static i32 CountSegments(const char *path, const char *segments[],
                         i32 max_segments) {
  i32 count = 0;
  const char *p = path;

  /* Handle root specially */
  if (p[0] == '/') {
    if (count < max_segments) {
      segments[count++] = p; /* Root segment "/" */
    }
    p++;
  }

  /* Find remaining segments */
  while (*p && count < max_segments) {
    /* Skip leading separators */
    while (*p == '/')
      p++;
    if (!*p)
      break;

    /* Mark start of segment */
    segments[count++] = p;

    /* Skip to next separator */
    while (*p && *p != '/')
      p++;
  }

  return count;
}

/* Calculate the visual width of a segment */
static i32 MeasureSegment(font *f, const char *segment_start,
                          i32 segment_length) {
  if (segment_length == 1 && segment_start[0] == '/') {
    return Font_MeasureWidth(f, "/");
  }
  /* Create a temporary string to measure */
  char temp[FS_MAX_NAME];
  if ((usize)segment_length >= sizeof(temp))
    segment_length = (i32)sizeof(temp) - 1;
  memcpy(temp, segment_start, segment_length);
  temp[segment_length] = '\0';
  return Font_MeasureWidth(f, temp);
}

/* ===== Public API ===== */

b32 Breadcrumb_GetPathForSegment(const char *path, i32 segment_index,
                                  char *out_buffer, usize buffer_size) {
  if (!path || !out_buffer || buffer_size == 0)
    return false;

  const char *segments[BREADCRUMB_MAX_SEGMENTS];
  i32 count = CountSegments(path, segments, BREADCRUMB_MAX_SEGMENTS);

  if (segment_index < 0 || segment_index >= count)
    return false;

  /* Calculate end position for this segment */
  const char *end;
  if (segment_index + 1 < count) {
    /* End at the start of next segment */
    end = segments[segment_index + 1];
    /* Don't include the leading slash of the next segment */
    if (*end == '/')
      end++;
  } else {
    /* Last segment - use end of string */
    end = path + strlen(path);
  }

  /* Calculate length */
  usize len = (usize)(end - path);
  if (len >= buffer_size)
    len = buffer_size - 1;

  memcpy(out_buffer, path, len);
  out_buffer[len] = '\0';

  return true;
}

breadcrumb_result Breadcrumb_Render(ui_context *ui, rect bounds,
                                     const char *path) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  ui_input *input = &ui->input;

  breadcrumb_result result = {0};
  result.clicked_segment = -1;

  /* Check if mouse is over breadcrumb */
  b32 is_hovered = UI_PointInRect(input->mouse_pos, bounds);

  /* Draw background */
  color bg_color = th->panel;
  if (is_hovered) {
    /* Subtle highlight on hover */
    bg_color = Color_Lighten(th->panel, 0.03f);
  }
  Render_DrawRect(ctx, bounds, bg_color);

  /* Parse path into segments */
  const char *segments[BREADCRUMB_MAX_SEGMENTS];
  i32 segment_count = CountSegments(path, segments, BREADCRUMB_MAX_SEGMENTS);

  /* Calculate segment information */
  typedef struct {
    const char *start;
    i32 length;
    i32 width;
    i32 x; /* Screen x position */
    i32 w; /* Screen width */
  } segment_info;

  segment_info segs[BREADCRUMB_MAX_SEGMENTS];
  i32 total_width = 0;

  for (i32 i = 0; i < segment_count; i++) {
    /* Calculate segment length (distance to next separator or end) */
    const char *p = segments[i];
    if (*p == '/' && (i == 0 || (p > path && p[-1] != '/'))) {
      /* Root segment - just "/" */
      segs[i].start = "/";
      segs[i].length = 1;
    } else {
      /* Find end of this segment */
      const char *end = p;
      while (*end && *end != '/')
        end++;
      segs[i].start = p;
      segs[i].length = (i32)(end - p);
    }

    segs[i].width = MeasureSegment(ui->font, segs[i].start, segs[i].length);
    total_width += segs[i].width;
    if (i < segment_count - 1) {
      /* Add separator width (except after last segment) */
      total_width += Font_MeasureWidth(ui->font, BREADCRUMB_SEPARATOR);
    }
    total_width += BREADCRUMB_SEGMENT_SPACING;
  }

  /* Determine starting position and whether we need truncation */
  i32 max_width = bounds.w - BREADCRUMB_PADDING * 2;
  i32 x = bounds.x + BREADCRUMB_PADDING;
  i32 y = bounds.y + (bounds.h - 16) / 2;

  i32 first_visible_segment = 0;
  b32 show_ellipsis = false;

  if (total_width > max_width && segment_count > 1) {
    /* Path is too long - show ellipsis and last segments that fit */
    i32 ellipsis_width = Font_MeasureWidth(ui->font, ".../");
    i32 available_width = max_width - ellipsis_width;

    /* Find which segments to show */
    i32 width_needed = 0;
    first_visible_segment = segment_count - 1;

    for (i32 i = segment_count - 1; i >= 0; i--) {
      i32 seg_width = segs[i].width;
      if (i < segment_count - 1) {
        seg_width += Font_MeasureWidth(ui->font, BREADCRUMB_SEPARATOR);
      }
      seg_width += BREADCRUMB_SEGMENT_SPACING;

      if (width_needed + seg_width <= available_width) {
        width_needed += seg_width;
        first_visible_segment = i;
      } else {
        break;
      }
    }

    show_ellipsis = true;
    x += ellipsis_width;
  }

  /* Draw ellipsis if needed */
  if (show_ellipsis) {
    v2i pos = {bounds.x + BREADCRUMB_PADDING, y};
    Render_DrawText(ctx, pos, "...", ui->font, th->text_muted);
  }

  /* Render each visible segment */
  for (i32 i = first_visible_segment; i < segment_count; i++) {
    /* Create temporary string for segment */
    char seg_text[FS_MAX_NAME];
    if (segs[i].length >= (i32)sizeof(seg_text))
      segs[i].length = (i32)sizeof(seg_text) - 1;
    memcpy(seg_text, segs[i].start, segs[i].length);
    seg_text[segs[i].length] = '\0';

    /* Calculate screen bounds for this segment */
    segs[i].x = x;
    segs[i].w = segs[i].width;

    /* Check if mouse is over this segment */
    rect seg_bounds = {x, bounds.y, segs[i].w, bounds.h};
    b32 seg_hovered = is_hovered && UI_PointInRect(input->mouse_pos, seg_bounds);

    /* Choose text color */
    color text_color = th->text;
    if (seg_hovered) {
      text_color = th->accent;
      /* Draw underline for hovered segment */
      rect underline = {x, bounds.y + bounds.h - 3, segs[i].w, 2};
      Render_DrawRect(ctx, underline, th->accent);
    } else if (i < segment_count - 1) {
      /* Earlier segments are slightly muted */
      text_color = th->text_muted;
    }

    /* Draw segment text */
    v2i pos = {x, y};
    Render_DrawText(ctx, pos, seg_text, ui->font, text_color);

    /* Advance x position */
    x += segs[i].w;

    /* Draw separator (except after last segment, and after root "/") */
    if (i < segment_count - 1) {
      /* Skip separator after root "/" segment to avoid "//" */
      b32 is_root_segment = (segs[i].length == 1 && segs[i].start[0] == '/');
      if (!is_root_segment) {
        v2i sep_pos = {x, y};
        Render_DrawText(ctx, sep_pos, BREADCRUMB_SEPARATOR, ui->font,
                        th->text_muted);
        x += Font_MeasureWidth(ui->font, BREADCRUMB_SEPARATOR);
      }
    }

    x += BREADCRUMB_SEGMENT_SPACING;
  }

  /* Detect which segment is hovered (if any) for click handling */
  i32 hovered_segment = -1;
  if (is_hovered) {
    for (i32 i = first_visible_segment; i < segment_count; i++) {
      rect seg_bounds = {segs[i].x, bounds.y, segs[i].w, bounds.h};
      if (UI_PointInRect(input->mouse_pos, seg_bounds)) {
        hovered_segment = i;
        break;
      }
    }
  }

  /* Handle click detection (single or double) anywhere in breadcrumb */
  b32 is_double_click = false;
  if (is_hovered && input->mouse_pressed[MOUSE_LEFT]) {
    u64 now = Platform_GetTimeMs();
    /* Check if this is a double-click (previous click was recent) */
    if (s_was_hovered && s_last_click_time > 0 &&
        (now - s_last_click_time) < BREADCRUMB_DOUBLE_CLICK_MS) {
      /* Double-click anywhere - copy full path to clipboard */
      Platform_SetClipboard(path);
      result.double_clicked = true;
      result.path_copied = true;
      s_copy_feedback_time = now;
      s_last_click_time = 0;
      is_double_click = true;
    } else {
      /* First click - start timer for potential double-click */
      s_last_click_time = now;
    }
  }

  /* Handle single-click navigation (only if not a double-click) */
  if (!is_double_click && hovered_segment >= 0 && input->mouse_pressed[MOUSE_LEFT]) {
    result.clicked_segment = hovered_segment;
  }

  s_was_hovered = is_hovered;

  /* Bottom border */
  {
    rect border = {bounds.x, bounds.y + bounds.h - 1, bounds.w, 1};
    Render_DrawRect(ctx, border, th->border);
  }

  /* Show copy feedback - draw last so it appears on top */
  {
    u64 now = Platform_GetTimeMs();
    if (s_copy_feedback_time > 0 &&
        (now - s_copy_feedback_time) < BREADCRUMB_COPY_FEEDBACK_MS) {
      const char *feedback = "Copied!";
      i32 fb_width = Font_MeasureWidth(ui->font, feedback);
      i32 fb_x = bounds.x + bounds.w - BREADCRUMB_PADDING - fb_width;
      i32 fb_y = bounds.y + (bounds.h - 16) / 2;

      /* Draw highlight background pill using accent color */
      rect fb_bg = {fb_x - 6, bounds.y + 6, fb_width + 12, bounds.h - 12};
      Render_DrawRectRounded(ctx, fb_bg, 4, th->accent);

      /* Draw feedback text using panel background color for contrast */
      v2i fb_pos = {fb_x, fb_y};
      Render_DrawText(ctx, fb_pos, feedback, ui->font, th->panel);
    }
  }

  return result;
}
