/*
 * notification.h - Toast Notification System for Workbench
 *
 * Non-intrusive, auto-dismissing notifications for errors, warnings, and info.
 * Appears at bottom-right corner, slides in/out with smooth animation.
 * C99, handmade hero style.
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "../ui.h"

/* ===== Configuration ===== */

#define NOTIFICATION_MAX_TEXT 256
#define NOTIFICATION_DURATION_MS 3000.0f  /* 3 seconds */
#define NOTIFICATION_SLIDE_IN_MS 250.0f
#define NOTIFICATION_SLIDE_OUT_MS 200.0f
#define NOTIFICATION_MAX_COUNT 4          /* Max notifications shown at once */

/* ===== Notification Types ===== */

typedef enum {
  NOTIFICATION_INFO,     /* Neutral - general information */
  NOTIFICATION_SUCCESS,  /* Green - operation completed */
  NOTIFICATION_WARNING,  /* Yellow/Orange - attention needed */
  NOTIFICATION_ERROR,    /* Red - operation failed */
  NOTIFICATION_COUNT
} notification_type;

/* ===== Notification State ===== */

typedef struct {
  char text[NOTIFICATION_MAX_TEXT];
  notification_type type;
  
  /* Animation state */
  f32 elapsed_ms;        /* Time since creation */
  f32 slide_progress;    /* 0 = off-screen, 1 = fully visible */
  b32 is_dismissing;     /* True when starting to slide out */
  b32 is_active;         /* True if this slot is in use */
} notification_item;

typedef struct {
  notification_item items[NOTIFICATION_MAX_COUNT];
  f32 stack_offset;      /* Current vertical offset for stacking */
} notification_state;

/* ===== API ===== */

/* Initialize notification system */
void Notification_Init(notification_state *state);

/* Update and render notifications. Call each frame.
 * bounds: Full window bounds for positioning */
void Notification_UpdateAndRender(notification_state *state, ui_context *ui, 
                                   rect bounds);

/* Show a new notification. Returns true if shown, false if queue full. */
b32 Notification_Show(notification_state *state, notification_type type, 
                       const char *format, ...);

/* Show a new notification with va_list (for internal use by wrappers) */
b32 Notification_ShowV(notification_state *state, notification_type type,
                        const char *format, va_list args);

/* Convenience wrappers */
static inline b32 Notification_Info(notification_state *state, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  b32 result = Notification_ShowV(state, NOTIFICATION_INFO, fmt, args);
  va_end(args);
  return result;
}

static inline b32 Notification_Success(notification_state *state, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  b32 result = Notification_ShowV(state, NOTIFICATION_SUCCESS, fmt, args);
  va_end(args);
  return result;
}

static inline b32 Notification_Warning(notification_state *state, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  b32 result = Notification_ShowV(state, NOTIFICATION_WARNING, fmt, args);
  va_end(args);
  return result;
}

static inline b32 Notification_Error(notification_state *state, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  b32 result = Notification_ShowV(state, NOTIFICATION_ERROR, fmt, args);
  va_end(args);
  return result;
}

/* Clear all active notifications */
void Notification_ClearAll(notification_state *state);

#endif /* NOTIFICATION_H */
