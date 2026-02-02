/*
 * ui.c - Immediate Mode UI Framework Implementation
 *
 * C99, handmade hero style.
 */

#include "ui.h"
#include <string.h>

/* ===== Global Context ===== */

ui_context *g_ui_ctx = NULL;

ui_context *UI_GetContext(void) { return g_ui_ctx; }

/* FNV-1a hash for string to ID */
static ui_id UI_HashString(const char *str) {
  ui_id hash = 2166136261u;
  while (*str) {
    hash ^= (ui_id)*str++;
    hash *= 16777619u;
  }
  return hash ? hash : 1; /* Avoid 0 (UI_ID_NONE) */
}

/* Combine ID with parent scope */
static ui_id UI_CombineID(ui_id parent, ui_id child) {
  return parent ^ (child * 16777619u);
}

ui_id UI_GenID(const char *str) {
  ui_context *ctx = g_ui_ctx;
  ui_id id = UI_HashString(str);

  /* Combine with ID stack */
  for (i32 i = 0; i < ctx->id_depth; i++) {
    id = UI_CombineID(ctx->id_stack[i], id);
  }
  return id;
}

void UI_PushID(const char *str) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->id_depth < UI_MAX_ID_STACK);
  ctx->id_stack[ctx->id_depth++] = UI_HashString(str);
}

void UI_PushIDInt(i32 n) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->id_depth < UI_MAX_ID_STACK);
  ctx->id_stack[ctx->id_depth++] = (ui_id)n;
}

void UI_PopID(void) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->id_depth > 0);
  ctx->id_depth--;
}

/* ===== Style System ===== */

void UI_PushStyleColor(ui_style_property prop, color c) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->style_depth < UI_MAX_STYLE_STACK);
  ctx->style_stack[ctx->style_depth].prop = prop;
  ctx->style_stack[ctx->style_depth].value.c = c;
  ctx->style_depth++;
}

void UI_PushStyleFloat(ui_style_property prop, f32 value) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->style_depth < UI_MAX_STYLE_STACK);
  ctx->style_stack[ctx->style_depth].prop = prop;
  ctx->style_stack[ctx->style_depth].value.f = value;
  ctx->style_depth++;
}

void UI_PushStyleInt(ui_style_property prop, i32 value) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->style_depth < UI_MAX_STYLE_STACK);
  ctx->style_stack[ctx->style_depth].prop = prop;
  ctx->style_stack[ctx->style_depth].value.i = value;
  ctx->style_depth++;
}

void UI_PopStyle(void) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->style_depth > 0);
  ctx->style_depth--;
}

void UI_PopStyleN(i32 n) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->style_depth >= n);
  ctx->style_depth -= n;
}

/* Get style, searching stack from top to bottom */
static ui_style_value UI_GetStyleValue(ui_style_property prop) {
  ui_context *ctx = g_ui_ctx;

  /* Search stack in reverse order */
  for (i32 i = ctx->style_depth - 1; i >= 0; i--) {
    if (ctx->style_stack[i].prop == prop) {
      return ctx->style_stack[i].value;
    }
  }

  /* Return default */
  return ctx->style_defaults[prop];
}

color UI_GetStyleColor(ui_style_property prop) {
  return UI_GetStyleValue(prop).c;
}

f32 UI_GetStyleFloat(ui_style_property prop) {
  return UI_GetStyleValue(prop).f;
}

i32 UI_GetStyleInt(ui_style_property prop) { return UI_GetStyleValue(prop).i; }

/* ===== Layout System ===== */

static ui_layout *UI_GetCurrentLayout(void) {
  ui_context *ctx = g_ui_ctx;
  if (ctx->layout_depth <= 0)
    return NULL;
  return &ctx->layout_stack[ctx->layout_depth - 1];
}

void UI_BeginLayout(ui_layout_direction dir, rect bounds) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->layout_depth < UI_MAX_LAYOUT_STACK);

  ui_layout *layout = &ctx->layout_stack[ctx->layout_depth++];
  layout->direction = dir;
  layout->bounds = bounds;
  layout->cursor = (v2i){bounds.x, bounds.y};
  layout->spacing = UI_GetStyleInt(UI_STYLE_SPACING);
  layout->max_cross = 0;
  layout->item_count = 0;
}

void UI_EndLayout(void) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->layout_depth > 0);
  ctx->layout_depth--;
}

void UI_BeginHorizontal(void) {
  rect avail = UI_GetAvailableRect();
  UI_BeginLayout(UI_LAYOUT_HORIZONTAL, avail);
}

void UI_EndHorizontal(void) {
  ui_context *ctx = g_ui_ctx;
  ui_layout *layout = UI_GetCurrentLayout();
  if (layout) {
    i32 height = layout->max_cross > 0 ? layout->max_cross : 20;
    UI_EndLayout();
    /* Advance parent layout */
    if (ctx->layout_depth > 0) {
      UI_AdvanceLayout(0, height);
    }
  }
}

void UI_BeginVertical(void) {
  rect avail = UI_GetAvailableRect();
  UI_BeginLayout(UI_LAYOUT_VERTICAL, avail);
}

void UI_EndVertical(void) {
  ui_context *ctx = g_ui_ctx;
  ui_layout *layout = UI_GetCurrentLayout();
  if (layout) {
    i32 width = layout->max_cross > 0 ? layout->max_cross : 20;
    UI_EndLayout();
    /* Advance parent layout */
    if (ctx->layout_depth > 0) {
      UI_AdvanceLayout(width, 0);
    }
  }
}

void UI_Spacer(i32 size) {
  ui_layout *layout = UI_GetCurrentLayout();
  if (!layout)
    return;

  if (layout->direction == UI_LAYOUT_HORIZONTAL) {
    layout->cursor.x += size;
  } else {
    layout->cursor.y += size;
  }
}

rect UI_GetAvailableRect(void) {
  ui_context *ctx = g_ui_ctx;

  if (ctx->layout_depth <= 0) {
    /* Full window */
    return (rect){0, 0, ctx->renderer->width, ctx->renderer->height};
  }

  ui_layout *layout = UI_GetCurrentLayout();

  if (layout->direction == UI_LAYOUT_HORIZONTAL) {
    return (rect){layout->cursor.x, layout->cursor.y,
                  layout->bounds.x + layout->bounds.w - layout->cursor.x,
                  layout->bounds.h};
  } else {
    return (rect){layout->cursor.x, layout->cursor.y, layout->bounds.w,
                  layout->bounds.y + layout->bounds.h - layout->cursor.y};
  }
}

void UI_AdvanceLayout(i32 width, i32 height) {
  ui_layout *layout = UI_GetCurrentLayout();
  if (!layout)
    return;

  if (layout->direction == UI_LAYOUT_HORIZONTAL) {
    layout->cursor.x += width;
    if (layout->item_count > 0) {
      layout->cursor.x += layout->spacing;
    }
    if (height > layout->max_cross) {
      layout->max_cross = height;
    }
  } else {
    layout->cursor.y += height;
    if (layout->item_count > 0) {
      layout->cursor.y += layout->spacing;
    }
    if (width > layout->max_cross) {
      layout->max_cross = width;
    }
  }
  layout->item_count++;
}

/* ===== Scroll Container ===== */

void UI_BeginScroll(v2i size, ui_scroll_state *state) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->scroll_depth < UI_MAX_SCROLL_STACK);

  rect avail = UI_GetAvailableRect();
  rect view = {avail.x, avail.y, size.x > 0 ? size.x : avail.w,
               size.y > 0 ? size.y : avail.h};

  /* Save scroll state on stack */
  ctx->scroll_stack[ctx->scroll_depth].clip = ctx->renderer->clip;
  ctx->scroll_stack[ctx->scroll_depth].view_rect = view;
  ctx->scroll_stack[ctx->scroll_depth].scroll_offset =
      (v2i){(i32)state->offset.x, (i32)state->offset.y};
  ctx->scroll_stack[ctx->scroll_depth].state = state;
  ctx->scroll_depth++;

  /* Update smooth scroll */
  SmoothValue_Update(&state->scroll_v, ctx->dt);
  SmoothValue_Update(&state->scroll_h, ctx->dt);
  state->offset.y = state->scroll_v.current;
  state->offset.x = state->scroll_h.current;

  /* Handle mouse wheel scrolling */
  if (UI_PointInRect(ctx->input.mouse_pos, view)) {
    if (ctx->input.scroll_delta != 0) {
      state->target_offset.y -= ctx->input.scroll_delta * 40.0f;
      /* Clamp to content bounds */
      if (state->target_offset.y < 0)
        state->target_offset.y = 0;
      f32 max_scroll = state->content_size.y - state->view_size.y;
      if (max_scroll < 0)
        max_scroll = 0;
      if (state->target_offset.y > max_scroll)
        state->target_offset.y = max_scroll;
      SmoothValue_SetTarget(&state->scroll_v, state->target_offset.y);
    }
  }

  state->view_size = (v2f){(f32)view.w, (f32)view.h};

  /* Set clip rect */
  Render_SetClipRect(ctx->renderer, view);

  /* Start nested layout with scroll offset */
  i32 scrollbar_w = UI_GetStyleInt(UI_STYLE_SCROLLBAR_WIDTH);
  rect content_bounds = {view.x, view.y - (i32)state->offset.y,
                         view.w - scrollbar_w, /* Leave space for scrollbar */
                         100000};              /* Large height for content */
  UI_BeginLayout(UI_LAYOUT_VERTICAL, content_bounds);
}

void UI_EndScroll(void) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->scroll_depth > 0);

  /* Get content size from layout */
  ui_layout *layout = UI_GetCurrentLayout();
  ctx->scroll_depth--;
  ui_scroll_state *state = ctx->scroll_stack[ctx->scroll_depth].state;
  rect view = ctx->scroll_stack[ctx->scroll_depth].view_rect;

  if (layout) {
    state->content_size.y =
        (f32)(layout->cursor.y - layout->bounds.y + layout->spacing);
    state->content_size.x =
        (f32)(layout->max_cross > 0 ? layout->max_cross : layout->bounds.w);
  }

  UI_EndLayout();

  /* Restore clip rect */
  Render_SetClipRect(ctx->renderer, ctx->scroll_stack[ctx->scroll_depth].clip);

  /* Draw scrollbar if needed */
  if (state->content_size.y > state->view_size.y) {
    f32 ratio = state->view_size.y / state->content_size.y;
    i32 bar_height = (i32)(view.h * ratio);
    if (bar_height < 20)
      bar_height = 20;

    f32 max_scroll = state->content_size.y - state->view_size.y;
    f32 scroll_ratio = max_scroll > 0 ? state->offset.y / max_scroll : 0;
    i32 bar_y = view.y + (i32)((view.h - bar_height) * scroll_ratio);

    i32 scrollbar_w = UI_GetStyleInt(UI_STYLE_SCROLLBAR_WIDTH);
    rect scrollbar = {view.x + view.w - scrollbar_w, bar_y, scrollbar_w,
                      bar_height};

    color bar_color = ctx->theme->text_muted;
    bar_color.a = 128;

    /* Highlight on hover */
    if (UI_PointInRect(ctx->input.mouse_pos, scrollbar)) {
      bar_color.a = 200;
    }

    Render_DrawRectRounded(ctx->renderer, scrollbar, 4.0f, bar_color);
  }
}

/* ===== Core API ===== */

void UI_Init(ui_context *ctx, render_context *renderer, const theme *th,
             font *f, font *mono_font) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->renderer = renderer;
  ctx->theme = th;
  ctx->font = f;
  ctx->mono_font = mono_font;

  /* Initialize default styles from theme */
  ctx->style_defaults[UI_STYLE_TEXT_COLOR].c = th->text;
  ctx->style_defaults[UI_STYLE_BG_COLOR].c = th->panel;
  ctx->style_defaults[UI_STYLE_BORDER_COLOR].c = th->border;
  ctx->style_defaults[UI_STYLE_ACCENT_COLOR].c = th->accent;
  ctx->style_defaults[UI_STYLE_HOVER_COLOR].c = th->accent_hover;
  ctx->style_defaults[UI_STYLE_ACTIVE_COLOR].c = th->accent_active;
  ctx->style_defaults[UI_STYLE_FOCUS_COLOR].c = th->accent;

  ctx->style_defaults[UI_STYLE_PADDING].i = th->spacing_sm;
  ctx->style_defaults[UI_STYLE_SPACING].i = th->spacing_sm;
  ctx->style_defaults[UI_STYLE_BORDER_WIDTH].f = 1.0f;
  ctx->style_defaults[UI_STYLE_BORDER_RADIUS].f = th->radius_sm;
  ctx->style_defaults[UI_STYLE_FONT_SIZE].i = th->font_size_md;
  ctx->style_defaults[UI_STYLE_SCROLLBAR_WIDTH].i = 10;

  ctx->style_defaults[UI_STYLE_MIN_WIDTH].i = 0;
  ctx->style_defaults[UI_STYLE_MIN_HEIGHT].i = 0;
  ctx->style_defaults[UI_STYLE_MAX_WIDTH].i = 10000;
  ctx->style_defaults[UI_STYLE_MAX_HEIGHT].i = 10000;

  /* Initialize hover animation */
  ctx->hover_anim.speed = 400.0f;

  g_ui_ctx = ctx;
}

void UI_Shutdown(ui_context *ctx) {
  if (ctx && g_ui_ctx == ctx) {
    g_ui_ctx = NULL;
  }
}

void UI_BeginFrame(ui_context *ctx, ui_input *input, f32 dt) {
  g_ui_ctx = ctx;
  ctx->input = *input;
  ctx->dt = dt;
  ctx->frame_count++;

  /* Reset layout stack */
  ctx->layout_depth = 0;
  ctx->id_depth = 0;
  ctx->scroll_depth = 0;

  /* Start with root layout */
  rect screen = {0, 0, ctx->renderer->width, ctx->renderer->height};
  UI_BeginLayout(UI_LAYOUT_VERTICAL, screen);

  /* Store last frame's focus */
  ctx->last_focused = ctx->focused;

  /* Reset focus tracking for this frame */
  ctx->focus_count = 0;

  /* Update modal state */
  ctx->active_modal = ctx->next_modal;
  ctx->next_modal = UI_ID_NONE; /* Reset for this frame, modals must call
                                   BeginModal to keep active */
  ctx->current_modal = UI_ID_NONE;
}

void UI_EndFrame(ui_context *ctx) {
  /* End root layout */
  while (ctx->layout_depth > 0) {
    UI_EndLayout();
  }

  /* Handle arrow key navigation - BLOCK if modal is active and we are not
     processing it? Actually, focus navigation might need to be restricted to
     modal only. For now, we let it be, assuming modal elements are the only
     ones getting focus.
  */

  if (ctx->input.key_pressed[KEY_DOWN]) {
    /* Move focus forward */
    if (ctx->focus_count > 0) {
      ctx->focus_index = (ctx->focus_index + 1) % ctx->focus_count;
      ctx->focused = ctx->focus_order[ctx->focus_index];
    }
  } else if (ctx->input.key_pressed[KEY_UP]) {
    /* Move focus backward */
    if (ctx->focus_count > 0) {
      ctx->focus_index =
          (ctx->focus_index - 1 + ctx->focus_count) % ctx->focus_count;
      ctx->focused = ctx->focus_order[ctx->focus_index];
    }
  }

  /* Clear pressed/released states (they're one-frame) */
  g_ui_ctx = NULL;
}

/* ===== Utility ===== */

b32 UI_PointInRect(v2i point, rect r) {
  return point.x >= r.x && point.x < r.x + r.w && point.y >= r.y &&
         point.y < r.y + r.h;
}

v2i UI_MeasureText(const char *text, font *f) {
  if (!f || !text)
    return (v2i){0, 0};
  return (v2i){Font_MeasureWidth(f, text), Font_GetLineHeight(f)};
}

/* ===== Focus Management ===== */

void UI_SetFocus(ui_id id) {
  ui_context *ctx = g_ui_ctx;
  ctx->focused = id;
}

void UI_ClearFocus(void) {
  ui_context *ctx = g_ui_ctx;
  ctx->focused = UI_ID_NONE;
}

b32 UI_HasFocus(ui_id id) { return g_ui_ctx->focused == id; }

b32 UI_IsHot(ui_id id) { return g_ui_ctx->hot == id; }

b32 UI_IsActive(ui_id id) { return g_ui_ctx->active == id; }

/* Register element in focus order */
void UI_RegisterFocusable(ui_id id) {
  ui_context *ctx = g_ui_ctx;
  /* If modal is active, only register elements if we are in that modal */
  if (ctx->active_modal != UI_ID_NONE &&
      ctx->current_modal != ctx->active_modal) {
    return;
  }

  if (ctx->focus_count < UI_MAX_FOCUS_ORDER) {
    ctx->focus_order[ctx->focus_count++] = id;
  }
}

/* Update hot/active state for an element */
b32 UI_UpdateInteraction(ui_id id, rect bounds) {
  ui_context *ctx = g_ui_ctx;

  /* Check modal blocking */
  if (ctx->active_modal != UI_ID_NONE) {
    if (ctx->current_modal != ctx->active_modal) {
      /* Block interaction */
      return false;
    }
  }

  /* Clip bounds to current clip rect - elements outside clip shouldn't receive
   * input */
  rect clip = ctx->renderer->clip;
  if (clip.w > 0 && clip.h > 0) {
    i32 left = bounds.x > clip.x ? bounds.x : clip.x;
    i32 top = bounds.y > clip.y ? bounds.y : clip.y;
    i32 right = (bounds.x + bounds.w) < (clip.x + clip.w)
                    ? (bounds.x + bounds.w)
                    : (clip.x + clip.w);
    i32 bottom = (bounds.y + bounds.h) < (clip.y + clip.h)
                     ? (bounds.y + bounds.h)
                     : (clip.y + clip.h);

    if (right <= left || bottom <= top) {
      /* Element is fully clipped - no interaction */
      return false;
    }
    bounds = (rect){left, top, right - left, bottom - top};
  }

  b32 hovered = UI_PointInRect(ctx->input.mouse_pos, bounds);
  b32 clicked = false;

  if (hovered) {
    ctx->hot = id;
    if (ctx->input.mouse_pressed[MOUSE_LEFT]) {
      ctx->active = id;
      ctx->focused = id;
    }
  }

  if (ctx->active == id) {
    if (ctx->input.mouse_released[MOUSE_LEFT]) {
      if (hovered) {
        clicked = true;
      }
      ctx->active = UI_ID_NONE;
    }
  }

  /* Keyboard activation */
  if (ctx->focused == id) {
    if (ctx->input.key_pressed[KEY_RETURN] ||
        ctx->input.key_pressed[KEY_SPACE]) {
      clicked = true;
    }
  }

  return clicked;
}

/* ===== Modal Management ===== */

void UI_BeginModal(const char *name) {
  ui_context *ctx = g_ui_ctx;
  ui_id id = UI_GenID(name);

  ctx->current_modal = id;
  ctx->next_modal = id; /* Keep alive for next frame */
}

void UI_EndModal(void) {
  ui_context *ctx = g_ui_ctx;
  ctx->current_modal = UI_ID_NONE;
}
