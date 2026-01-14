/*
 * animation.h - Animation system for Workbench
 *
 * Lerp utilities, easing functions, and animation state tracking.
 * C99, handmade hero style.
 */

#ifndef ANIMATION_H
#define ANIMATION_H

#include "../renderer/renderer.h"
#include "types.h"

/* ===== Basic Interpolation ===== */

/* Linear interpolation (integer) */
static inline i32 Lerpi(i32 a, i32 b, f32 t) {
  return a + (i32)((f32)(b - a) * t);
}

/* Linear interpolation (float) */
static inline f32 Lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

/* Linear interpolation (color) */
static inline color LerpColor(color a, color b, f32 t) {
  return (color){(u8)Lerpi(a.r, b.r, t), (u8)Lerpi(a.g, b.g, t),
                 (u8)Lerpi(a.b, b.b, t), (u8)Lerpi(a.a, b.a, t)};
}

/* ===== Easing Functions ===== */
/* All take t in [0, 1] and return value in [0, 1] */

/* Linear (no easing) */
static inline f32 Ease_Linear(f32 t) { return t; }

/* Quadratic ease out (decelerating) */
static inline f32 Ease_OutQuad(f32 t) { return t * (2.0f - t); }

/* Quadratic ease in (accelerating) */
static inline f32 Ease_InQuad(f32 t) { return t * t; }

/* Quadratic ease in-out */
static inline f32 Ease_InOutQuad(f32 t) {
  if (t < 0.5f) {
    return 2.0f * t * t;
  }
  return 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
}

/* Cubic ease out */
static inline f32 Ease_OutCubic(f32 t) {
  f32 inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

/* Cubic ease in */
static inline f32 Ease_InCubic(f32 t) { return t * t * t; }

/* Exponential ease out */
static inline f32 Ease_OutExpo(f32 t) {
  return t >= 1.0f
             ? 1.0f
             : 1.0f - (1.0f / 1024.0f) * (1.0f - t < 0.001f ? 1024.0f : 1.0f);
}

/* ===== Animation State ===== */

typedef enum { ANIM_IDLE = 0, ANIM_RUNNING, ANIM_FINISHED } animation_status;

typedef struct {
  f32 start_value;
  f32 end_value;
  f32 current_value;
  f32 duration_ms;
  f32 elapsed_ms;
  animation_status status;
  f32 (*easing)(f32 t); /* Easing function pointer */
} animation_state;

/* ===== Animation API ===== */

/* Start an animation */
void Animation_Start(animation_state *anim, f32 from, f32 to, f32 duration_ms,
                     f32 (*easing)(f32));

/* Update animation (call each frame with delta time in ms) */
void Animation_Update(animation_state *anim, f32 dt_ms);

/* Reset animation to idle */
void Animation_Reset(animation_state *anim);

/* Get normalized progress (0-1) */
f32 Animation_GetProgress(animation_state *anim);

/* Check if animation is complete */
b32 Animation_IsFinished(animation_state *anim);

/* ===== Smooth Value (for hover effects, etc.) ===== */

typedef struct {
  f32 current;
  f32 target;
  f32 speed; /* Units per second */
} smooth_value;

/* Update smooth value (dt in seconds) */
void SmoothValue_Update(smooth_value *sv, f32 dt);

/* Set target value */
void SmoothValue_SetTarget(smooth_value *sv, f32 target);

/* Set immediate value (no smoothing) */
void SmoothValue_SetImmediate(smooth_value *sv, f32 value);

/* Global flag to enable/disable animations */
extern b32 g_animations_enabled;

#endif /* ANIMATION_H */
