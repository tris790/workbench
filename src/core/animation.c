/*
 * animation.c - Animation system implementation for Workbench
 *
 * C99, handmade hero style.
 */

#include "animation.h"

b32 g_animations_enabled = true;

/* ===== Animation API ===== */

void Animation_Start(animation_state *anim, f32 from, f32 to, f32 duration_ms,
                     f32 (*easing)(f32)) {
  anim->start_value = from;
  anim->end_value = to;
  anim->current_value = from;
  anim->duration_ms = duration_ms;
  anim->elapsed_ms = 0.0f;
  anim->status = WB_ANIM_RUNNING;
  anim->easing = easing ? easing : Ease_Linear;
}

void Animation_Update(animation_state *anim, f32 dt_ms) {
  if (anim->status != WB_ANIM_RUNNING)
    return;

  if (!g_animations_enabled) {
    anim->elapsed_ms = anim->duration_ms;
    anim->current_value = anim->end_value;
    anim->status = WB_ANIM_FINISHED;
    return;
  }

  anim->elapsed_ms += dt_ms;

  if (anim->elapsed_ms >= anim->duration_ms) {
    anim->elapsed_ms = anim->duration_ms;
    anim->current_value = anim->end_value;
    anim->status = WB_ANIM_FINISHED;
    return;
  }

  f32 t = anim->elapsed_ms / anim->duration_ms;
  f32 eased_t = anim->easing(t);
  anim->current_value = Lerpf(anim->start_value, anim->end_value, eased_t);
}

void Animation_Reset(animation_state *anim) {
  anim->current_value = anim->start_value;
  anim->elapsed_ms = 0.0f;
  anim->status = WB_ANIM_IDLE;
}

f32 Animation_GetProgress(animation_state *anim) {
  if (anim->duration_ms <= 0.0f)
    return 1.0f;
  f32 progress = anim->elapsed_ms / anim->duration_ms;
  if (progress < 0.0f)
    return 0.0f;
  if (progress > 1.0f)
    return 1.0f;
  return progress;
}

b32 Animation_IsFinished(animation_state *anim) {
  return anim->status == WB_ANIM_FINISHED;
}

/* ===== Smooth Value ===== */

void SmoothValue_Update(smooth_value *sv, f32 dt) {
  if (sv->current == sv->target)
    return;

  if (!g_animations_enabled) {
    sv->current = sv->target;
    return;
  }

  f32 diff = sv->target - sv->current;
  f32 step = sv->speed * dt;

  if (diff > 0) {
    sv->current += step;
    if (sv->current > sv->target) {
      sv->current = sv->target;
    }
  } else {
    sv->current -= step;
    if (sv->current < sv->target) {
      sv->current = sv->target;
    }
  }
}

void SmoothValue_SetTarget(smooth_value *sv, f32 target) {
  sv->target = target;
}

void SmoothValue_SetImmediate(smooth_value *sv, f32 value) {
  sv->current = value;
  sv->target = value;
}
