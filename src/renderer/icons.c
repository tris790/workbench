/*
 * icons.c - Simple Geometric Icons Implementation
 *
 * C99, handmade hero style.
 */

#include "icons.h"
#include "../core/theme.h"

/* ===== Helper: Draw a simple line (1px thick rect) ===== */

static void DrawLine(render_context *ctx, i32 x1, i32 y1, i32 x2, i32 y2,
                     color c) {
  if (x1 == x2) {
    /* Vertical line */
    i32 top = y1 < y2 ? y1 : y2;
    i32 h = (y1 < y2 ? y2 - y1 : y1 - y2) + 1;
    Render_DrawRect(ctx, (rect){x1, top, 1, h}, c);
  } else if (y1 == y2) {
    /* Horizontal line */
    i32 left = x1 < x2 ? x1 : x2;
    i32 w = (x1 < x2 ? x2 - x1 : x1 - x2) + 1;
    Render_DrawRect(ctx, (rect){left, y1, w, 1}, c);
  }
  /* Diagonal lines not supported for simplicity */
}

/* ===== Icon Drawing Functions ===== */

static void DrawFolderIcon(render_context *ctx, rect b, color c) {
  /* Folder shape: rectangle with tab on top-left */
  i32 tab_width = b.w / 3;
  i32 tab_height = b.h / 4;

  /* Main body */
  rect body = {b.x, b.y + tab_height, b.w, b.h - tab_height};
  Render_DrawRectRounded(ctx, body, 2.0f, c);

  /* Tab on top */
  rect tab = {b.x, b.y, tab_width, tab_height + 2};
  Render_DrawRectRounded(ctx, tab, 2.0f, c);
}

static void DrawFileIcon(render_context *ctx, rect b, color c) {
  /* Document shape: rectangle with folded corner */
  i32 fold = b.w / 4;

  /* Main body */
  Render_DrawRectRounded(ctx, b, 1.0f, c);

  /* Corner fold indicator (slightly darker top-right) */
  color fold_color = Color_Darken(c, 0.2f);
  rect fold_rect = {b.x + b.w - fold, b.y, fold, fold};
  Render_DrawRect(ctx, fold_rect, fold_color);
}

static void DrawCodeIcon(render_context *ctx, rect b, color c,
                         const char *symbol) {
  /* Document with code symbol */
  DrawFileIcon(ctx, b, c);

  /* Draw "</>" or similar in center - simplified as just the base icon */
  (void)symbol;
}

static void DrawImageIcon(render_context *ctx, rect b, color c) {
  /* Picture frame with simple landscape */
  Render_DrawRectRounded(ctx, b, 2.0f, c);

  /* Inner "image" area */
  i32 margin = 2;
  rect inner = {b.x + margin, b.y + margin, b.w - margin * 2, b.h - margin * 2};

  color dark = Color_Darken(c, 0.3f);

  /* Simple mountain/landscape shape */
  i32 mid_y = inner.y + inner.h / 2;

  /* Draw a small circle (sun) */
  rect sun = {inner.x + inner.w - 4, inner.y + 2, 3, 3};
  Render_DrawRect(ctx, sun, dark);

  /* Draw horizontal line (horizon) */
  DrawLine(ctx, inner.x, mid_y, inner.x + inner.w - 1, mid_y, dark);
}

static void DrawArchiveIcon(render_context *ctx, rect b, color c) {
  /* Box with bands */
  Render_DrawRectRounded(ctx, b, 2.0f, c);

  /* Horizontal bands */
  color band = Color_Darken(c, 0.3f);
  i32 band_h = 2;
  DrawLine(ctx, b.x + 2, b.y + b.h / 3, b.x + b.w - 3, b.y + b.h / 3, band);
  DrawLine(ctx, b.x + 2, b.y + (b.h * 2) / 3, b.x + b.w - 3,
           b.y + (b.h * 2) / 3, band);
  (void)band_h;
}

static void DrawAudioIcon(render_context *ctx, rect b, color c) {
  /* Music note shape - simplified to a filled circle with stem */
  Render_DrawRectRounded(ctx, b, 2.0f, c);

  color dark = Color_Darken(c, 0.3f);

  /* Note head (small filled area) */
  rect note = {b.x + b.w / 4, b.y + b.h * 2 / 3, b.w / 3, b.h / 4};
  Render_DrawRect(ctx, note, dark);

  /* Stem */
  DrawLine(ctx, b.x + b.w / 4 + b.w / 3 - 1, b.y + b.h / 4,
           b.x + b.w / 4 + b.w / 3 - 1, b.y + b.h * 2 / 3 + b.h / 8, dark);
}

static void DrawVideoIcon(render_context *ctx, rect b, color c) {
  /* Film/video frame */
  Render_DrawRectRounded(ctx, b, 2.0f, c);

  color dark = Color_Darken(c, 0.3f);

  /* Sprocket holes on sides */
  i32 hole_size = 2;
  for (i32 i = 0; i < 3; i++) {
    i32 y = b.y + 2 + i * (b.h / 3);
    rect left_hole = {b.x + 1, y, hole_size, hole_size};
    rect right_hole = {b.x + b.w - hole_size - 1, y, hole_size, hole_size};
    Render_DrawRect(ctx, left_hole, dark);
    Render_DrawRect(ctx, right_hole, dark);
  }
}

static void DrawConfigIcon(render_context *ctx, rect b, color c) {
  /* Gear shape - simplified to nested squares */
  Render_DrawRectRounded(ctx, b, 2.0f, c);

  color dark = Color_Darken(c, 0.3f);

  /* Inner gear teeth (corners) */
  i32 m = 3;
  rect inner = {b.x + m, b.y + m, b.w - m * 2, b.h - m * 2};
  Render_DrawRect(ctx, inner, dark);

  /* Center hole */
  rect center = {b.x + b.w / 2 - 1, b.y + b.h / 2 - 1, 3, 3};
  Render_DrawRect(ctx, center, c);
}

static void DrawMarkdownIcon(render_context *ctx, rect b, color c) {
  /* Document with "M" indicator */
  DrawFileIcon(ctx, b, c);

  /* We could draw "M" but keeping it simple */
}

static void DrawSymlinkIcon(render_context *ctx, rect b, color c) {
  /* Arrow/link indicator */
  DrawFileIcon(ctx, b, c);

  color dark = Color_Darken(c, 0.3f);

  /* Small arrow in corner */
  DrawLine(ctx, b.x + 2, b.y + b.h - 4, b.x + b.w / 2, b.y + b.h - 4, dark);
  DrawLine(ctx, b.x + b.w / 2 - 2, b.y + b.h - 6, b.x + b.w / 2, b.y + b.h - 4,
           dark);
}

static void DrawExecutableIcon(render_context *ctx, rect b, color c) {
  /* Terminal/gear - simplified box with ">" */
  Render_DrawRectRounded(ctx, b, 2.0f, c);

  color dark = Color_Darken(c, 0.4f);

  /* Draw ">" shape */
  i32 cx = b.x + b.w / 3;
  i32 cy = b.y + b.h / 2;
  DrawLine(ctx, cx, cy - 3, cx + 3, cy, dark);
  DrawLine(ctx, cx, cy + 3, cx + 3, cy, dark);
}

/* ===== Public API ===== */

void Icon_Draw(render_context *ctx, rect bounds, file_icon_type type, color c) {
  switch (type) {
  case WB_FILE_ICON_DIRECTORY:
    DrawFolderIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_CODE_C:
  case WB_FILE_ICON_CODE_H:
  case WB_FILE_ICON_CODE_PY:
  case WB_FILE_ICON_CODE_JS:
  case WB_FILE_ICON_CODE_OTHER:
    DrawCodeIcon(ctx, bounds, c, "");
    break;

  case WB_FILE_ICON_IMAGE:
    DrawImageIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_DOCUMENT:
    DrawFileIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_ARCHIVE:
    DrawArchiveIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_AUDIO:
    DrawAudioIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_VIDEO:
    DrawVideoIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_CONFIG:
    DrawConfigIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_MARKDOWN:
    DrawMarkdownIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_SYMLINK:
    DrawSymlinkIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_EXECUTABLE:
    DrawExecutableIcon(ctx, bounds, c);
    break;

  case WB_FILE_ICON_FILE:
  case WB_FILE_ICON_UNKNOWN:
  default:
    DrawFileIcon(ctx, bounds, c);
    break;
  }
}

color Icon_GetTypeColor(file_icon_type type, const theme *t) {
  switch (type) {
  case WB_FILE_ICON_DIRECTORY:
    return t->accent;

  case WB_FILE_ICON_CODE_C:
  case WB_FILE_ICON_CODE_H:
    return COLOR_RGB(86, 156, 214); /* C/C++ blue */

  case WB_FILE_ICON_CODE_PY:
    return COLOR_RGB(55, 118, 171); /* Python blue-ish */

  case WB_FILE_ICON_CODE_JS:
    return COLOR_RGB(241, 224, 90); /* JavaScript yellow */

  case WB_FILE_ICON_CODE_OTHER:
    return COLOR_RGB(156, 220, 254); /* Light blue */

  case WB_FILE_ICON_IMAGE:
    return COLOR_RGB(197, 134, 192); /* Purple-ish */

  case WB_FILE_ICON_AUDIO:
    return COLOR_RGB(206, 145, 120); /* Orange-brown */

  case WB_FILE_ICON_VIDEO:
    return COLOR_RGB(220, 89, 89); /* Red */

  case WB_FILE_ICON_ARCHIVE:
    return COLOR_RGB(215, 186, 125); /* Tan/brown */

  case WB_FILE_ICON_MARKDOWN:
    return COLOR_RGB(78, 201, 176); /* Teal */

  case WB_FILE_ICON_CONFIG:
    return COLOR_RGB(156, 220, 254); /* Light blue */

  case WB_FILE_ICON_EXECUTABLE:
    return COLOR_RGB(78, 201, 176); /* Teal/green */

  case WB_FILE_ICON_SYMLINK:
    return t->text_muted;

  case WB_FILE_ICON_DOCUMENT:
    return t->text;

  case WB_FILE_ICON_FILE:
  case WB_FILE_ICON_UNKNOWN:
  default:
    return t->text_muted;
  }
}
