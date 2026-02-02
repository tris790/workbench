/*
 * widgets.c - Basic UI Widgets Implementation
 */

#include "widgets.h"
#include "../ui.h"

b32 UI_Button(const char *label) {
  ui_id id = UI_GenID(label);

  /* Register for focus navigation */
  UI_RegisterFocusable(id);

  /* Calculate size */
  i32 padding = UI_GetStyleInt(UI_STYLE_PADDING);
  v2i text_size = UI_MeasureText(label, g_ui_ctx->font);
  i32 width = text_size.x + padding * 2;
  i32 height = text_size.y + padding * 2;

  /* Apply min/max */
  i32 min_w = UI_GetStyleInt(UI_STYLE_MIN_WIDTH);
  i32 min_h = UI_GetStyleInt(UI_STYLE_MIN_HEIGHT);
  if (width < min_w)
    width = min_w;
  if (height < min_h)
    height = min_h;

  rect avail = UI_GetAvailableRect();
  rect bounds = {avail.x, avail.y, width, height};

  /* Interaction */
  b32 clicked = UI_UpdateInteraction(id, bounds);

  /* Determine colors based on state */
  color bg = UI_GetStyleColor(UI_STYLE_BG_COLOR);
  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  f32 radius = UI_GetStyleFloat(UI_STYLE_BORDER_RADIUS);

  if (g_ui_ctx->active == id) {
    bg = UI_GetStyleColor(UI_STYLE_ACTIVE_COLOR);
    text_color = g_ui_ctx->theme->background;
  } else if (g_ui_ctx->hot == id) {
    bg = UI_GetStyleColor(UI_STYLE_HOVER_COLOR);
    text_color = g_ui_ctx->theme->background;
  }

  /* Draw */
  Render_DrawRectRounded(g_ui_ctx->renderer, bounds, radius, bg);

  /* Focus ring */
  if (g_ui_ctx->focused == id) {
    color focus = UI_GetStyleColor(UI_STYLE_FOCUS_COLOR);
    focus.a = 128;
    rect focus_rect = {bounds.x - 2, bounds.y - 2, bounds.w + 4, bounds.h + 4};
    Render_DrawRectRounded(g_ui_ctx->renderer, focus_rect, radius + 1, focus);
    /* Redraw button on top */
    Render_DrawRectRounded(g_ui_ctx->renderer, bounds, radius, bg);
  }

  /* Text centered */
  v2i text_pos = {bounds.x + (bounds.w - text_size.x) / 2,
                  bounds.y + (bounds.h - text_size.y) / 2};
  Render_DrawText(g_ui_ctx->renderer, text_pos, label, g_ui_ctx->font,
                  text_color);

  /* Advance layout */
  UI_AdvanceLayout(width, height);

  return clicked;
}

void UI_Label(const char *text) {
  v2i text_size = UI_MeasureText(text, g_ui_ctx->font);
  rect avail = UI_GetAvailableRect();
  v2i pos = {avail.x, avail.y};

  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  Render_DrawText(g_ui_ctx->renderer, pos, text, g_ui_ctx->font, text_color);

  UI_AdvanceLayout(text_size.x, text_size.y);
}

b32 UI_Selectable(const char *label, b32 selected) {
  ui_id id = UI_GenID(label);

  /* Register for focus navigation */
  UI_RegisterFocusable(id);

  /* Calculate size */
  i32 padding = UI_GetStyleInt(UI_STYLE_PADDING);
  v2i text_size = UI_MeasureText(label, g_ui_ctx->font);
  rect avail = UI_GetAvailableRect();

  i32 height = text_size.y + padding * 2;
  rect bounds = {avail.x, avail.y, avail.w, height};

  /* Interaction */
  b32 clicked = UI_UpdateInteraction(id, bounds);

  /* Determine colors */
  color bg = (color){0, 0, 0, 0}; /* Transparent by default */
  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  f32 radius = UI_GetStyleFloat(UI_STYLE_BORDER_RADIUS);

  if (selected || g_ui_ctx->focused == id) {
    bg = UI_GetStyleColor(UI_STYLE_ACCENT_COLOR);
    text_color = g_ui_ctx->theme->background;
  } else if (g_ui_ctx->hot == id) {
    bg = g_ui_ctx->theme->panel_alt;
  }

  /* Draw background */
  if (bg.a > 0) {
    Render_DrawRectRounded(g_ui_ctx->renderer, bounds, radius, bg);
  }

  /* Text */
  v2i text_pos = {bounds.x + padding, bounds.y + padding};
  Render_DrawText(g_ui_ctx->renderer, text_pos, label, g_ui_ctx->font,
                  text_color);

  /* Advance layout */
  UI_AdvanceLayout(bounds.w, height);

  return clicked;
}

void UI_Separator(void) {
  rect avail = UI_GetAvailableRect();

  i32 spacing = UI_GetStyleInt(UI_STYLE_SPACING);
  rect line = {avail.x, avail.y + spacing / 2, avail.w, 1};

  color border = UI_GetStyleColor(UI_STYLE_BORDER_COLOR);
  Render_DrawRect(g_ui_ctx->renderer, line, border);

  UI_AdvanceLayout(avail.w, spacing);
}

void UI_DrawPanel(rect bounds) {
  const theme *th = g_ui_ctx->theme;

  /* Border */
  rect border = {bounds.x - 1, bounds.y - 1, bounds.w + 2, bounds.h + 2};
  Render_DrawRectRounded(g_ui_ctx->renderer, border, th->radius_md + 1,
                         th->border);

  /* Background on top of border */
  Render_DrawRectRounded(g_ui_ctx->renderer, bounds, th->radius_md, th->panel);
}
