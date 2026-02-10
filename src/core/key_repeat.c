/*
 * key_repeat.c - Frame-independent Key Repeat System Implementation
 *
 * C99, handmade hero style.
 */

#include "key_repeat.h"
#include <string.h>

/* Per-key repeat state */
typedef struct {
  u64 press_time;       /* When key was first pressed (0 if not pressed) */
  u64 last_repeat_time; /* When the last repeat fired */
  b32 fired_this_frame; /* Whether this key should fire this frame */
  b32 is_repeating;     /* Whether we've passed the initial delay */
  u32 character; /* Character associated with this key (for text repeat) */
} key_repeat_state;

/* Global state for all keys */
static key_repeat_state g_key_states[WB_KEY_COUNT] = {0};

/* Current frame's repeated text input (0 if none) */
static u32 g_repeated_text_input = 0;

void KeyRepeat_Init(void) {
  memset(g_key_states, 0, sizeof(g_key_states));
  g_repeated_text_input = 0;
}

void KeyRepeat_Update(b32 *key_down, b32 *key_pressed, u64 current_time_ms) {
  /* Reset repeated text input for this frame */
  g_repeated_text_input = 0;

  for (i32 i = 0; i < WB_KEY_COUNT; i++) {
    key_repeat_state *state = &g_key_states[i];

    /* Clear fire flag from previous frame */
    state->fired_this_frame = false;

    if (key_pressed[i]) {
      /* Key was just pressed this frame */
      state->press_time = current_time_ms;
      state->last_repeat_time = current_time_ms;
      state->is_repeating = false;
      state->fired_this_frame = true;
      /* Character will be set separately via KeyRepeat_SetCharacter */
    } else if (key_down[i] && state->press_time > 0) {
      /* Key is held down */
      u64 held_duration = current_time_ms - state->press_time;

      if (!state->is_repeating) {
        /* Check if we should start repeating */
        if (held_duration >= WB_KEY_REPEAT_DELAY_MS) {
          state->is_repeating = true;
          state->last_repeat_time = current_time_ms;
          state->fired_this_frame = true;
          /* Set repeated text input if this key has a character */
          if (state->character > 0) {
            g_repeated_text_input = state->character;
          }
        }
      } else {
        /* Already repeating - check if it's time for another repeat */
        u64 since_last_repeat = current_time_ms - state->last_repeat_time;
        if (since_last_repeat >= WB_KEY_REPEAT_RATE_MS) {
          state->last_repeat_time = current_time_ms;
          state->fired_this_frame = true;
          /* Set repeated text input if this key has a character */
          if (state->character > 0) {
            g_repeated_text_input = state->character;
          }
        }
      }
    } else {
      /* Key is not down - reset state */
      state->press_time = 0;
      state->last_repeat_time = 0;
      state->is_repeating = false;
      state->character = 0;
    }
  }
}

b32 KeyRepeat_Check(key_code key) {
  if (key >= WB_KEY_COUNT)
    return false;
  return g_key_states[key].fired_this_frame;
}

void KeyRepeat_SetCharacter(key_code key, u32 character) {
  if (key >= WB_KEY_COUNT)
    return;
  g_key_states[key].character = character;
}

u32 KeyRepeat_GetTextInput(void) { return g_repeated_text_input; }

void KeyRepeat_Reset(key_code key) {
  if (key >= WB_KEY_COUNT)
    return;
  g_key_states[key].fired_this_frame = false;
}

void KeyRepeat_ResetAll(void) {
  for (i32 i = 0; i < WB_KEY_COUNT; i++) {
    g_key_states[i].fired_this_frame = false;
  }
}
