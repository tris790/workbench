/*
 * ui.c - Immediate Mode UI Framework Implementation
 *
 * C99, handmade hero style.
 */

#include "ui.h"
#include "../core/input.h"
#include "../core/text.h"
#include <stdio.h>
#include <string.h>

/* ===== Global Context ===== */

static ui_context *g_ui_ctx = NULL;

ui_context *UI_GetContext(void) { return g_ui_ctx; }

/* ===== ID Generation ===== */

/* FNV-1a hash for string to ID */
static ui_id HashString(const char *str) {
  ui_id hash = 2166136261u;
  while (*str) {
    hash ^= (ui_id)*str++;
    hash *= 16777619u;
  }
  return hash ? hash : 1; /* Avoid 0 (UI_ID_NONE) */
}

/* Combine ID with parent scope */
static ui_id CombineID(ui_id parent, ui_id child) {
  return parent ^ (child * 16777619u);
}

ui_id UI_GenID(const char *str) {
  ui_context *ctx = g_ui_ctx;
  ui_id id = HashString(str);

  /* Combine with ID stack */
  for (i32 i = 0; i < ctx->id_depth; i++) {
    id = CombineID(ctx->id_stack[i], id);
  }
  return id;
}

void UI_PushID(const char *str) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->id_depth < UI_MAX_ID_STACK);
  ctx->id_stack[ctx->id_depth++] = HashString(str);
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
static ui_style_value GetStyleValue(ui_style_property prop) {
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

color UI_GetStyleColor(ui_style_property prop) { return GetStyleValue(prop).c; }

f32 UI_GetStyleFloat(ui_style_property prop) { return GetStyleValue(prop).f; }

i32 UI_GetStyleInt(ui_style_property prop) { return GetStyleValue(prop).i; }

/* ===== Layout System ===== */

static ui_layout *GetCurrentLayout(void) {
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
  ui_layout *layout = GetCurrentLayout();
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
  ui_layout *layout = GetCurrentLayout();
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
  ui_layout *layout = GetCurrentLayout();
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

  ui_layout *layout = GetCurrentLayout();

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
  ui_layout *layout = GetCurrentLayout();
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

#define SCROLLBAR_WIDTH 10

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
  rect content_bounds = {view.x, view.y - (i32)state->offset.y,
                         view.w -
                             SCROLLBAR_WIDTH, /* Leave space for scrollbar */
                         view.h * 10};        /* Large height for content */
  UI_BeginLayout(UI_LAYOUT_VERTICAL, content_bounds);
}

void UI_EndScroll(void) {
  ui_context *ctx = g_ui_ctx;
  Assert(ctx->scroll_depth > 0);

  /* Get content size from layout */
  ui_layout *layout = GetCurrentLayout();
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

    rect scrollbar = {view.x + view.w - SCROLLBAR_WIDTH, bar_y, SCROLLBAR_WIDTH,
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
             font *main_font, font *mono_font) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->renderer = renderer;
  ctx->theme = th;
  ctx->main_font = main_font;
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

  ctx->style_defaults[UI_STYLE_MIN_WIDTH].i = 0;
  ctx->style_defaults[UI_STYLE_MIN_HEIGHT].i = 0;
  ctx->style_defaults[UI_STYLE_MAX_WIDTH].i = 10000;
  ctx->style_defaults[UI_STYLE_MAX_HEIGHT].i = 10000;

  /* Initialize hover animation */
  ctx->hover_anim.speed = 400.0f;

  g_ui_ctx = ctx;
}

void UI_Shutdown(ui_context *ctx) {
  if (g_ui_ctx == ctx) {
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
static void RegisterFocusable(ui_id id) {
  ui_context *ctx = g_ui_ctx;
  /* If modal is active, only register elements if we are in that modal */
  if (ctx->active_modal != UI_ID_NONE &&
      ctx->current_modal != ctx->active_modal) {
    return;
  }

  if (ctx->focus_count < 256) {
    ctx->focus_order[ctx->focus_count++] = id;
  }
}

/* Update hot/active state for an element */
static b32 UpdateInteraction(ui_id id, rect bounds) {
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

/* ===== Widgets ===== */

b32 UI_Button(const char *label) {
  ui_context *ctx = g_ui_ctx;
  ui_id id = UI_GenID(label);

  /* Register for focus navigation */
  RegisterFocusable(id);

  /* Calculate size */
  i32 padding = UI_GetStyleInt(UI_STYLE_PADDING);
  v2i text_size = UI_MeasureText(label, ctx->main_font);
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
  b32 clicked = UpdateInteraction(id, bounds);

  /* Determine colors based on state */
  color bg = UI_GetStyleColor(UI_STYLE_BG_COLOR);
  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  f32 radius = UI_GetStyleFloat(UI_STYLE_BORDER_RADIUS);

  if (ctx->active == id) {
    bg = UI_GetStyleColor(UI_STYLE_ACTIVE_COLOR);
    text_color = ctx->theme->background;
  } else if (ctx->hot == id) {
    bg = UI_GetStyleColor(UI_STYLE_HOVER_COLOR);
    text_color = ctx->theme->background;
  }

  /* Draw */
  Render_DrawRectRounded(ctx->renderer, bounds, radius, bg);

  /* Focus ring */
  if (ctx->focused == id) {
    color focus = UI_GetStyleColor(UI_STYLE_FOCUS_COLOR);
    focus.a = 128;
    rect focus_rect = {bounds.x - 2, bounds.y - 2, bounds.w + 4, bounds.h + 4};
    Render_DrawRectRounded(ctx->renderer, focus_rect, radius + 1, focus);
    /* Redraw button on top */
    Render_DrawRectRounded(ctx->renderer, bounds, radius, bg);
  }

  /* Text centered */
  v2i text_pos = {bounds.x + (bounds.w - text_size.x) / 2,
                  bounds.y + (bounds.h - text_size.y) / 2};
  Render_DrawText(ctx->renderer, text_pos, label, ctx->main_font, text_color);

  /* Advance layout */
  UI_AdvanceLayout(width, height);

  return clicked;
}

void UI_Label(const char *text) {
  ui_context *ctx = g_ui_ctx;

  v2i text_size = UI_MeasureText(text, ctx->main_font);
  rect avail = UI_GetAvailableRect();
  v2i pos = {avail.x, avail.y};

  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  Render_DrawText(ctx->renderer, pos, text, ctx->main_font, text_color);

  UI_AdvanceLayout(text_size.x, text_size.y);
}

b32 UI_Selectable(const char *label, b32 selected) {
  ui_context *ctx = g_ui_ctx;
  ui_id id = UI_GenID(label);

  /* Register for focus navigation */
  RegisterFocusable(id);

  /* Calculate size */
  i32 padding = UI_GetStyleInt(UI_STYLE_PADDING);
  v2i text_size = UI_MeasureText(label, ctx->main_font);
  rect avail = UI_GetAvailableRect();

  i32 height = text_size.y + padding * 2;
  rect bounds = {avail.x, avail.y, avail.w, height};

  /* Interaction */
  b32 clicked = UpdateInteraction(id, bounds);

  /* Determine colors */
  color bg = (color){0, 0, 0, 0}; /* Transparent by default */
  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  f32 radius = UI_GetStyleFloat(UI_STYLE_BORDER_RADIUS);

  if (selected || ctx->focused == id) {
    bg = UI_GetStyleColor(UI_STYLE_ACCENT_COLOR);
    text_color = ctx->theme->background;
  } else if (ctx->hot == id) {
    bg = ctx->theme->panel_alt;
  }

  /* Draw background */
  if (bg.a > 0) {
    Render_DrawRectRounded(ctx->renderer, bounds, radius, bg);
  }

  /* Text */
  v2i text_pos = {bounds.x + padding, bounds.y + padding};
  Render_DrawText(ctx->renderer, text_pos, label, ctx->main_font, text_color);

  /* Advance layout */
  UI_AdvanceLayout(bounds.w, height);

  return clicked;
}

void UI_Separator(void) {
  ui_context *ctx = g_ui_ctx;
  rect avail = UI_GetAvailableRect();

  i32 spacing = UI_GetStyleInt(UI_STYLE_SPACING);
  rect line = {avail.x, avail.y + spacing / 2, avail.w, 1};

  color border = UI_GetStyleColor(UI_STYLE_BORDER_COLOR);
  Render_DrawRect(ctx->renderer, line, border);

  UI_AdvanceLayout(avail.w, spacing);
}

/* ===== Text Input ===== */

/* Helper: push undo state */
static void TextInputPushUndo(ui_text_state *state, const char *text) {
  if (state->undo_count < UI_MAX_UNDO_STATES) {
    i32 idx = state->undo_index;
    strncpy(state->undo_stack[idx].text, text, UI_MAX_TEXT_INPUT_SIZE - 1);
    state->undo_stack[idx].cursor_pos = state->cursor_pos;
    state->undo_index = (idx + 1) % UI_MAX_UNDO_STATES;
    state->undo_count++;
  }
}

/* Helper: pop undo state */
static b32 TextInputPopUndo(ui_text_state *state, char *buffer, i32 buf_size) {
  if (state->undo_count > 0) {
    state->undo_count--;
    state->undo_index =
        (state->undo_index - 1 + UI_MAX_UNDO_STATES) % UI_MAX_UNDO_STATES;
    strncpy(buffer, state->undo_stack[state->undo_index].text,
            (size_t)buf_size - 1);
    state->cursor_pos = state->undo_stack[state->undo_index].cursor_pos;
    return true;
  }
  return false;
}

/* Process text input logic (shared by widgets) */
b32 UI_ProcessTextInput(ui_text_state *state, char *buffer, i32 buffer_size,
                        ui_input *input) {
  b32 changed = false;
  i32 text_len =
      (i32)strlen(buffer); /* Assumes UTF-8 well-formedness for length */

  /* Handle cursor blink reset on input */
  if (input->text_input || input->key_pressed[KEY_LEFT] ||
      input->key_pressed[KEY_RIGHT] || input->key_pressed[KEY_BACKSPACE] ||
      input->key_pressed[KEY_DELETE] || input->key_pressed[KEY_HOME] ||
      input->key_pressed[KEY_END]) {
    state->cursor_blink = 0.0f;
  }

  b32 ctrl = (input->modifiers & MOD_CTRL) != 0;
  b32 shift = (input->modifiers & MOD_SHIFT) != 0;

  /* Ctrl+A - Select all */
  if (ctrl && input->key_pressed[KEY_A]) {
    state->selection_start = 0;
    state->selection_end = Text_UTF8Length(buffer); /* CHARS */
    state->cursor_pos = state->selection_end;
  }

  /* Ctrl+C - Copy */
  if (ctrl && input->key_pressed[KEY_C]) {
    if (state->selection_start >= 0) {
      i32 start = state->selection_start < state->selection_end
                      ? state->selection_start
                      : state->selection_end;
      i32 end = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;
      i32 start_byte = Text_UTF8ByteOffset(buffer, start);
      i32 end_byte = Text_UTF8ByteOffset(buffer, end);
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      i32 len = end_byte - start_byte;
      if (len >= UI_MAX_TEXT_INPUT_SIZE)
        len = UI_MAX_TEXT_INPUT_SIZE - 1;

      strncpy(temp, buffer + start_byte, (size_t)len);
      temp[len] = '\0';
      Platform_SetClipboard(temp);
    }
  }

  /* Ctrl+X - Cut */
  if (ctrl && input->key_pressed[KEY_X]) {
    if (state->selection_start >= 0) {
      i32 start = state->selection_start < state->selection_end
                      ? state->selection_start
                      : state->selection_end;
      i32 end = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;
      i32 start_byte = Text_UTF8ByteOffset(buffer, start);
      i32 end_byte = Text_UTF8ByteOffset(buffer, end);

      /* Copy to clipboard */
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      i32 len = end_byte - start_byte;
      if (len >= UI_MAX_TEXT_INPUT_SIZE)
        len = UI_MAX_TEXT_INPUT_SIZE - 1;

      strncpy(temp, buffer + start_byte, (size_t)len);
      temp[len] = '\0';
      Platform_SetClipboard(temp);

      /* Delete selection */
      TextInputPushUndo(state, buffer);

      i32 total_bytes = (i32)strlen(buffer);
      memmove(buffer + start_byte, buffer + end_byte,
              (size_t)(total_bytes - end_byte + 1));

      state->cursor_pos = start;
      state->selection_start = -1;
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Ctrl+V - Paste */
  if (ctrl && input->key_pressed[KEY_V]) {
    char clipboard[UI_MAX_TEXT_INPUT_SIZE];
    if (Platform_GetClipboard(clipboard, sizeof(clipboard))) {
      /* Delete selection first if any */
      if (state->selection_start >= 0) {
        i32 start = state->selection_start < state->selection_end
                        ? state->selection_start
                        : state->selection_end;
        i32 end = state->selection_start > state->selection_end
                      ? state->selection_start
                      : state->selection_end;
        i32 start_byte = Text_UTF8ByteOffset(buffer, start);
        i32 end_byte = Text_UTF8ByteOffset(buffer, end);
        TextInputPushUndo(state, buffer);

        i32 total_bytes = (i32)strlen(buffer);
        memmove(buffer + start_byte, buffer + end_byte,
                (size_t)(total_bytes - end_byte + 1));

        state->cursor_pos = start;
        state->selection_start = -1;
        text_len = (i32)strlen(buffer);
      }

      /* Insert */
      i32 paste_len = (i32)strlen(clipboard);
      if (text_len + paste_len < buffer_size - 1) {
        TextInputPushUndo(state, buffer);
        i32 cursor_byte = Text_UTF8ByteOffset(buffer, state->cursor_pos);
        i32 total_bytes = (i32)strlen(buffer);

        memmove(buffer + cursor_byte + paste_len, buffer + cursor_byte,
                (size_t)(total_bytes - cursor_byte + 1));
        memcpy(buffer + cursor_byte, clipboard, (size_t)paste_len);
        state->cursor_pos +=
            Text_UTF8Length(clipboard); /* Advance char count */
        changed = true;
        text_len = (i32)strlen(buffer);
      }
    }
  }

  /* Ctrl+Z - Undo */
  if (ctrl && input->key_pressed[KEY_Z]) {
    if (TextInputPopUndo(state, buffer, buffer_size)) {
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Arrow keys for cursor movement */
  if (input->key_pressed[KEY_LEFT] || Input_KeyRepeat(KEY_LEFT)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }

    if (ctrl) {
      /* Word jump left */
      if (state->cursor_pos > 0) {
        i32 cursor = state->cursor_pos - 1;
        /* Skip whitespace backwards */
        while (cursor > 0 &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ') {
          cursor--;
        }
        /* Skip non-whitespace backwards */
        while (cursor > 0 &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] != ' ') {
          cursor--;
        }
        /* If we stopped on space (and not at start), move forward one to be at
         * start of word */
        if (cursor > 0 || buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ') {
          if (buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ')
            cursor++;
        }
        state->cursor_pos = cursor;
      }
    } else {
      if (state->cursor_pos > 0) {
        state->cursor_pos--;
      }
    }

    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  if (input->key_pressed[KEY_RIGHT] || Input_KeyRepeat(KEY_RIGHT)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }

    if (ctrl) {
      /* Word jump right */
      i32 char_count = Text_UTF8Length(buffer);
      if (state->cursor_pos < char_count) {
        i32 cursor = state->cursor_pos;
        /* Skip non-whitespace forward */
        while (cursor < char_count &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] != ' ') {
          cursor++;
        }
        /* Skip whitespace forward */
        while (cursor < char_count &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ') {
          cursor++;
        }
        state->cursor_pos = cursor;
      }
    } else {
      if (state->cursor_pos < Text_UTF8Length(buffer)) {
        state->cursor_pos++;
      }
    }

    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  /* Home/End */
  if (input->key_pressed[KEY_HOME] || Input_KeyRepeat(KEY_HOME)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }
    state->cursor_pos = 0;
    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  if (input->key_pressed[KEY_END] || Input_KeyRepeat(KEY_END)) {
    if (shift && state->selection_start < 0) {
      state->selection_start = state->cursor_pos;
    }
    state->cursor_pos = Text_UTF8Length(buffer);
    if (shift) {
      state->selection_end = state->cursor_pos;
    } else {
      state->selection_start = -1;
    }
  }

  /* Backspace - check both local key_pressed and global key repeat */
  if (input->key_pressed[KEY_BACKSPACE] || Input_KeyRepeat(KEY_BACKSPACE)) {
    if (state->selection_start >= 0) {
      /* Delete selection */
      i32 start = state->selection_start < state->selection_end
                      ? state->selection_start
                      : state->selection_end;
      i32 end = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;
      i32 start_byte = Text_UTF8ByteOffset(buffer, start);
      i32 end_byte = Text_UTF8ByteOffset(buffer, end);
      TextInputPushUndo(state, buffer);

      i32 total_bytes = (i32)strlen(buffer);
      memmove(buffer + start_byte, buffer + end_byte,
              (size_t)(total_bytes - end_byte + 1));
      state->cursor_pos = start;
      state->selection_start = -1;
      changed = true;
      text_len = (i32)strlen(buffer);
    } else if (state->cursor_pos > 0) {
      TextInputPushUndo(state, buffer);

      i32 target_pos = state->cursor_pos - 1;

      if (ctrl) {
        /* Word delete backward */
        i32 cursor = state->cursor_pos - 1;
        /* Skip whitespace backwards */
        while (cursor > 0 &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ') {
          cursor--;
        }
        /* Skip non-whitespace backwards */
        while (cursor > 0 &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] != ' ') {
          cursor--;
        }
        /* Adjust to delete from start of word */
        if (cursor > 0 || buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ') {
          if (buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ')
            cursor++;
        }
        target_pos = cursor;
      }

      i32 byte_pos = Text_UTF8ByteOffset(buffer, state->cursor_pos);
      i32 target_byte = Text_UTF8ByteOffset(buffer, target_pos);
      i32 total_bytes = (i32)strlen(buffer);

      memmove(buffer + target_byte, buffer + byte_pos,
              (size_t)(total_bytes - byte_pos + 1));
      state->cursor_pos = target_pos;
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Delete - check both local key_pressed and global key repeat */
  if (input->key_pressed[KEY_DELETE] || Input_KeyRepeat(KEY_DELETE)) {
    if (state->selection_start >= 0) {
      /* Same as backspace with selection */
      i32 start = state->selection_start < state->selection_end
                      ? state->selection_start
                      : state->selection_end;
      i32 end = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;
      i32 start_byte = Text_UTF8ByteOffset(buffer, start);
      i32 end_byte = Text_UTF8ByteOffset(buffer, end);
      TextInputPushUndo(state, buffer);

      i32 total_bytes = (i32)strlen(buffer);
      memmove(buffer + start_byte, buffer + end_byte,
              (size_t)(total_bytes - end_byte + 1));
      state->cursor_pos = start;
      state->selection_start = -1;
      changed = true;
      text_len = (i32)strlen(buffer);
    } else if (state->cursor_pos < Text_UTF8Length(buffer)) {
      TextInputPushUndo(state, buffer);

      i32 target_pos = state->cursor_pos + 1;

      if (ctrl) {
        /* Word delete forward */
        i32 char_count = Text_UTF8Length(buffer);
        i32 cursor = state->cursor_pos;
        /* Skip non-whitespace forward */
        while (cursor < char_count &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] != ' ') {
          cursor++;
        }
        /* Skip whitespace forward */
        while (cursor < char_count &&
               buffer[Text_UTF8ByteOffset(buffer, cursor)] == ' ') {
          cursor++;
        }
        target_pos = cursor;
      }

      i32 byte_pos = Text_UTF8ByteOffset(buffer, state->cursor_pos);
      i32 target_byte = Text_UTF8ByteOffset(buffer, target_pos);
      i32 total_bytes = (i32)strlen(buffer);

      memmove(buffer + byte_pos, buffer + target_byte,
              (size_t)(total_bytes - target_byte + 1));
      changed = true;
      text_len = (i32)strlen(buffer);
    }
  }

  /* Text input */
  if (input->text_input && input->text_input >= 32) {
    /* Delete selection first if any */
    if (state->selection_start >= 0) {
      i32 start = state->selection_start < state->selection_end
                      ? state->selection_start
                      : state->selection_end;
      i32 end = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;
      i32 start_byte = Text_UTF8ByteOffset(buffer, start);
      i32 end_byte = Text_UTF8ByteOffset(buffer, end);
      TextInputPushUndo(state, buffer);

      i32 total_bytes = (i32)strlen(buffer);
      memmove(buffer + start_byte, buffer + end_byte,
              (size_t)(total_bytes - end_byte + 1));
      state->cursor_pos = start;
      state->selection_start = -1;
      text_len = (i32)strlen(buffer);
    }

    /* Insert character */
    if (text_len < buffer_size - 2) {
      /* Simple ASCII for now */
      if (input->text_input < 128) {
        i32 byte_pos = Text_UTF8ByteOffset(buffer, state->cursor_pos);
        /* Use fresh length */
        i32 total_bytes = (i32)strlen(buffer);
        memmove(buffer + byte_pos + 1, buffer + byte_pos,
                (size_t)(total_bytes - byte_pos + 1));
        buffer[byte_pos] = (char)input->text_input;
        state->cursor_pos++;
        changed = true;
      }
    }
  }

  return changed;
}

b32 UI_TextInput(char *buffer, i32 buffer_size, const char *placeholder,
                 ui_text_state *state) {
  ui_context *ctx = g_ui_ctx;
  ui_id id = UI_GenID(placeholder ? placeholder : "##textinput");

  RegisterFocusable(id);

  i32 padding = UI_GetStyleInt(UI_STYLE_PADDING);
  i32 font_height = Font_GetLineHeight(ctx->main_font);
  i32 height = font_height + padding * 2;

  rect avail = UI_GetAvailableRect();
  rect bounds = {avail.x, avail.y, avail.w, height};

  /* Interaction */
  b32 hovered = UI_PointInRect(ctx->input.mouse_pos, bounds);
  b32 changed = false;

  if (hovered && ctx->input.mouse_pressed[MOUSE_LEFT]) {
    ctx->focused = id;
    state->has_focus = true;

    /* Calculate cursor position from click */
    i32 click_x = ctx->input.mouse_pos.x - bounds.x - padding;
    i32 text_len = Text_UTF8Length(buffer);
    i32 best_pos = 0;
    i32 best_dist = 10000;

    for (i32 i = 0; i <= text_len; i++) {
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      strncpy(temp, buffer, (size_t)Text_UTF8ByteOffset(buffer, i));
      temp[Text_UTF8ByteOffset(buffer, i)] = '\0';
      i32 w = Font_MeasureWidth(ctx->main_font, temp);
      i32 dist = click_x > w ? click_x - w : w - click_x;
      if (dist < best_dist) {
        best_dist = dist;
        best_pos = i;
      }
    }
    state->cursor_pos = best_pos;
    state->selection_start = -1;
  }

  /* Focus management */
  if (ctx->focused == id) {
    state->has_focus = true;
  } else if (state->has_focus) {
    /* Check if we just lost focus or if we are claiming it */
    if (ctx->last_focused != id) {
      /* We weren't focused last frame, but state says we should be.
         Claim focus. */
      ctx->focused = id;
    } else {
      /* We were focused and lost it. */
      state->has_focus = false;
      state->selection_start = -1;
    }
  }

  /* Handle keyboard input when focused */
  if (ctx->focused == id) {
    if (UI_ProcessTextInput(state, buffer, buffer_size, &ctx->input)) {
      changed = true;
    }

    /* Update cursor blink */
    if (!g_animations_enabled) {
      state->cursor_blink = 0.0f; /* Always show cursor */
    } else {
      state->cursor_blink += ctx->dt * 2.0f;
      if (state->cursor_blink > 2.0f)
        state->cursor_blink -= 2.0f;
    }
  }

  /* Drawing */
  color bg = ctx->theme->panel_alt;
  color text_color = UI_GetStyleColor(UI_STYLE_TEXT_COLOR);
  color border_color = UI_GetStyleColor(UI_STYLE_BORDER_COLOR);
  f32 radius = UI_GetStyleFloat(UI_STYLE_BORDER_RADIUS);

  /* Focus highlight */
  if (ctx->focused == id) {
    border_color = UI_GetStyleColor(UI_STYLE_ACCENT_COLOR);
  }

  /* Draw border first */
  Render_DrawRectRounded(ctx->renderer, bounds, radius, border_color);

  /* Background (inset by 1px for border) */
  rect inner = {bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2};
  Render_DrawRectRounded(ctx->renderer, inner, radius - 1, bg);

  /* Set clip for text */
  rect text_clip = {bounds.x + padding, bounds.y, bounds.w - padding * 2,
                    bounds.h};
  Render_SetClipRect(ctx->renderer, text_clip);

  v2i text_pos = {bounds.x + padding, bounds.y + padding};
  b32 show_placeholder = (buffer[0] == '\0');

  if (show_placeholder && placeholder) {
    color placeholder_color = ctx->theme->text_muted;
    Render_DrawText(ctx->renderer, text_pos, placeholder, ctx->main_font,
                    placeholder_color);
  } else {
    /* Draw selection highlight */
    if (state->selection_start >= 0 && ctx->focused == id) {
      i32 start = state->selection_start < state->selection_end
                      ? state->selection_start
                      : state->selection_end;
      i32 end = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;

      char temp[UI_MAX_TEXT_INPUT_SIZE];
      strncpy(temp, buffer, (size_t)Text_UTF8ByteOffset(buffer, start));
      temp[Text_UTF8ByteOffset(buffer, start)] = '\0';
      i32 start_x = text_pos.x + Font_MeasureWidth(ctx->main_font, temp);

      strncpy(temp, buffer, (size_t)Text_UTF8ByteOffset(buffer, end));
      temp[Text_UTF8ByteOffset(buffer, end)] = '\0';
      i32 end_x = text_pos.x + Font_MeasureWidth(ctx->main_font, temp);

      rect sel_rect = {start_x, bounds.y + 2, end_x - start_x, bounds.h - 4};
      color sel_color = UI_GetStyleColor(UI_STYLE_ACCENT_COLOR);
      sel_color.a = 128;
      Render_DrawRect(ctx->renderer, sel_rect, sel_color);
    }

    /* Draw text */
    Render_DrawText(ctx->renderer, text_pos, buffer, ctx->main_font,
                    text_color);

    /* Draw cursor */
    if (ctx->focused == id && state->cursor_blink < 1.0f) {
      char temp[UI_MAX_TEXT_INPUT_SIZE];
      strncpy(temp, buffer,
              (size_t)Text_UTF8ByteOffset(buffer, state->cursor_pos));
      temp[Text_UTF8ByteOffset(buffer, state->cursor_pos)] = '\0';
      i32 cursor_x = text_pos.x + Font_MeasureWidth(ctx->main_font, temp);

      rect cursor_rect = {cursor_x, bounds.y + 3, 2, bounds.h - 6};
      Render_DrawRect(ctx->renderer, cursor_rect, text_color);
    }
  }

  /* Restore clip */
  Render_ResetClipRect(ctx->renderer);

  UI_AdvanceLayout(bounds.w, height);

  return changed;
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

/* ===== Panels ===== */

void UI_DrawPanel(rect bounds) {
  ui_context *ctx = g_ui_ctx;
  const theme *th = ctx->theme;

  /* Background */
  Render_DrawRectRounded(ctx->renderer, bounds, th->radius_md, th->panel);

  /* Border */
  rect border = {bounds.x - 1, bounds.y - 1, bounds.w + 2, bounds.h + 2};
  Render_DrawRectRounded(ctx->renderer, border, th->radius_md + 1, th->border);
  Render_DrawRectRounded(ctx->renderer, bounds, th->radius_md, th->panel);
}
