/*
 * notification.c - Toast Notification System Implementation
 *
 * C99, handmade hero style.
 */

#include "notification.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ===== Configuration ===== */

#define NOTIFICATION_WIDTH 320
#define NOTIFICATION_MIN_HEIGHT 32
#define NOTIFICATION_PADDING_X 12
#define NOTIFICATION_PADDING_Y 6
#define NOTIFICATION_MARGIN_RIGHT 24
#define NOTIFICATION_MARGIN_BOTTOM 24
#define NOTIFICATION_SPACING 8

/* Icon size for notification type indicator */
#define NOTIFICATION_ICON_SIZE 16

/* ===== Type Colors ===== */

typedef struct {
  color bg;
  color border;
  color icon;
  color text;
} notification_style;

static notification_style GetNotificationStyle(const theme *th, 
                                                notification_type type) {
  notification_style style = {0};
  
  switch (type) {
    case NOTIFICATION_INFO:
      /* Subtle blue-gray */
      style.bg = Color_WithAlpha(th->panel, 245);
      style.border = Color_WithAlpha(th->accent, 80);
      style.icon = th->accent;
      style.text = th->text;
      break;
      
    case NOTIFICATION_SUCCESS:
      /* Green-tinted */
      style.bg = Color_WithAlpha(th->panel, 245);
      style.border = Color_WithAlpha(th->success, 100);
      style.icon = th->success;
      style.text = th->text;
      break;
      
    case NOTIFICATION_WARNING:
      /* Yellow/orange-tinted */
      style.bg = Color_WithAlpha(th->panel, 245);
      style.border = Color_WithAlpha(th->warning, 100);
      style.icon = th->warning;
      style.text = th->text;
      break;
      
    case NOTIFICATION_ERROR:
      /* Red-tinted */
      style.bg = Color_WithAlpha(th->panel, 245);
      style.border = Color_WithAlpha(th->error, 100);
      style.icon = th->error;
      style.text = th->text;
      break;
      
    default:
      style.bg = Color_WithAlpha(th->panel, 245);
      style.border = Color_WithAlpha(th->border, 100);
      style.icon = th->text_muted;
      style.text = th->text;
      break;
  }
  
  return style;
}

/* ===== Icon Rendering ===== */

/* Simple icon characters that work with both renderers.
 * Using text-based icons avoids needing line/circle primitives in OpenGL. */

static const char *GetNotificationIconText(notification_type type) {
  switch (type) {
    case NOTIFICATION_INFO:    return "i";
    case NOTIFICATION_SUCCESS: return "✓";
    case NOTIFICATION_WARNING: return "!";
    case NOTIFICATION_ERROR:   return "✕";
    default: return "";
  }
}

/* ===== Public API ===== */

void Notification_Init(notification_state *state) {
  memset(state, 0, sizeof(*state));
}

b32 Notification_ShowV(notification_state *state, notification_type type,
                        const char *format, va_list args) {
  /* Find first available slot */
  notification_item *item = NULL;
  for (i32 i = 0; i < NOTIFICATION_MAX_COUNT; i++) {
    if (!state->items[i].is_active) {
      item = &state->items[i];
      break;
    }
  }
  
  if (!item) {
    /* Queue full - try to reuse the oldest dismissing notification */
    for (i32 i = 0; i < NOTIFICATION_MAX_COUNT; i++) {
      if (state->items[i].is_dismissing) {
        item = &state->items[i];
        break;
      }
    }
  }
  
  if (!item) {
    return false; /* No slot available */
  }
  
  /* Format message */
  vsnprintf(item->text, sizeof(item->text), format, args);
  
  /* Ensure null termination */
  item->text[NOTIFICATION_MAX_TEXT - 1] = '\0';
  
  /* Initialize state */
  item->type = type;
  item->elapsed_ms = 0.0f;
  item->slide_progress = 0.0f;
  item->is_dismissing = false;
  item->is_active = true;
  
  return true;
}

b32 Notification_Show(notification_state *state, notification_type type,
                       const char *format, ...) {
  va_list args;
  va_start(args, format);
  b32 result = Notification_ShowV(state, type, format, args);
  va_end(args);
  return result;
}

void Notification_ClearAll(notification_state *state) {
  for (i32 i = 0; i < NOTIFICATION_MAX_COUNT; i++) {
    state->items[i].is_dismissing = true;
  }
}

void Notification_UpdateAndRender(notification_state *state, ui_context *ui,
                                   rect bounds) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  f32 dt_ms = ui->dt * 1000.0f;
  
  /* Calculate starting position (bottom-right corner) */
  i32 start_x = bounds.x + bounds.w - NOTIFICATION_WIDTH - NOTIFICATION_MARGIN_RIGHT;
  i32 start_y = bounds.y + bounds.h - NOTIFICATION_MARGIN_BOTTOM;
  
  i32 current_y = start_y;
  i32 active_count = 0;
  
  /* Process from newest to oldest so newer notifications appear on top */
  for (i32 i = NOTIFICATION_MAX_COUNT - 1; i >= 0; i--) {
    notification_item *item = &state->items[i];
    
    if (!item->is_active) {
      continue;
    }
    
    active_count++;
    
    /* Update timing */
    item->elapsed_ms += dt_ms;
    
    /* Calculate slide progress */
    if (!item->is_dismissing) {
      /* Sliding in or holding */
      if (item->elapsed_ms < NOTIFICATION_SLIDE_IN_MS) {
        /* Sliding in */
        f32 t = item->elapsed_ms / NOTIFICATION_SLIDE_IN_MS;
        item->slide_progress = Ease_OutCubic(t);
      } else if (item->elapsed_ms < NOTIFICATION_SLIDE_IN_MS + NOTIFICATION_DURATION_MS) {
        /* Holding - fully visible */
        item->slide_progress = 1.0f;
      } else {
        /* Time to dismiss */
        item->is_dismissing = true;
      }
    } else {
      /* Sliding out */
      f32 dismiss_time = item->elapsed_ms - NOTIFICATION_SLIDE_IN_MS - NOTIFICATION_DURATION_MS;
      if (dismiss_time < 0) {
        dismiss_time = 0; /* Started dismissing early via ClearAll */
      }
      
      if (dismiss_time >= NOTIFICATION_SLIDE_OUT_MS) {
        /* Finished sliding out */
        item->is_active = false;
        continue;
      }
      
      f32 t = dismiss_time / NOTIFICATION_SLIDE_OUT_MS;
      item->slide_progress = 1.0f - Ease_InCubic(t);
    }
    
    /* Calculate notification height - always single line */
    i32 text_w = NOTIFICATION_WIDTH - NOTIFICATION_PADDING_X * 2 - NOTIFICATION_ICON_SIZE - 8;
    
    /* Single line notifications only */
    i32 notif_h = NOTIFICATION_MIN_HEIGHT;
    
    /* Calculate position with slide animation */
    i32 slide_offset = (i32)((1.0f - item->slide_progress) * (NOTIFICATION_WIDTH + NOTIFICATION_MARGIN_RIGHT));
    i32 notif_x = start_x + slide_offset;
    i32 notif_y = current_y - notif_h;
    
    rect notif_rect = {notif_x, notif_y, NOTIFICATION_WIDTH, notif_h};
    
    /* Draw notification */
    notification_style style = GetNotificationStyle(th, item->type);
    
    /* Background panel with slight transparency */
    Render_DrawRectRounded(ctx, notif_rect, th->radius_md, style.bg);
    
    /* Border - using slightly larger rect behind for compatibility */
    rect border_rect = {notif_rect.x - 1, notif_rect.y - 1, 
                        notif_rect.w + 2, notif_rect.h + 2};
    Render_DrawRectRounded(ctx, border_rect, th->radius_md + 1, style.border);
    
    /* Left border accent */
    rect accent_rect = {notif_rect.x, notif_rect.y + 3, 3, notif_rect.h - 6};
    Render_DrawRectRounded(ctx, accent_rect, 1.5f, style.icon);
    
    /* Icon (text-based for renderer compatibility) */
    const char *icon_text = GetNotificationIconText(item->type);
    v2i icon_size = UI_MeasureText(icon_text, ui->font);
    v2i icon_pos = {
      notif_rect.x + NOTIFICATION_PADDING_X,
      notif_rect.y + (notif_h - icon_size.y) / 2
    };
    Render_DrawText(ctx, icon_pos, icon_text, ui->font, style.icon);
    
    /* Text */
    i32 text_x = notif_rect.x + NOTIFICATION_PADDING_X + NOTIFICATION_ICON_SIZE + 8;
    i32 text_y = notif_rect.y + (notif_h - Font_GetLineHeight(ui->font)) / 2;
    
    /* Draw text with potential truncation */
    char display_text[NOTIFICATION_MAX_TEXT];
    strncpy(display_text, item->text, sizeof(display_text));
    display_text[sizeof(display_text) - 1] = '\0';
    
    /* Simple truncation with ellipsis if needed */
    v2i measured = UI_MeasureText(display_text, ui->font);
    if (measured.x > text_w) {
      /* Truncate and add ellipsis */
      usize len = strlen(display_text);
      while (len > 0 && UI_MeasureText(display_text, ui->font).x > text_w - 12) {
        display_text[--len] = '\0';
      }
      if (len > 0) {
        strncat(display_text, "...", sizeof(display_text) - len - 1);
      }
    }
    
    v2i text_pos = {text_x, text_y};
    Render_DrawText(ctx, text_pos, display_text, ui->font, style.text);
    
    /* Update position for next notification */
    current_y = notif_y - NOTIFICATION_SPACING;
  }
  
  state->stack_offset = (f32)(start_y - current_y);
}
