/*
 * key_repeat.h - Frame-independent Key Repeat System
 *
 * Implements OS-like key repeat behavior for immediate mode GUI.
 * Uses wall-clock timing to be independent of frame rate.
 * C99, handmade hero style.
 */

#ifndef WB_KEY_REPEAT_H
#define WB_KEY_REPEAT_H

#include "platform.h"
#include "types.h"

/* ===== Configuration ===== */

/* Initial delay before key starts repeating (milliseconds) */
#define WB_KEY_REPEAT_DELAY_MS 500

/* Interval between repeats once repeating starts (milliseconds) */
#define WB_KEY_REPEAT_RATE_MS 30

/* ===== API ===== */

/* Initialize key repeat system - call once at startup */
void KeyRepeat_Init(void);

/* Update key repeat state - call once per frame with current key states */
void KeyRepeat_Update(b32 *key_down, b32 *key_pressed, u64 current_time_ms);

/* Check if a key should fire this frame (initial press OR repeat) */
b32 KeyRepeat_Check(key_code key);

/* Set the character for a key (call on initial press) */
void KeyRepeat_SetCharacter(key_code key, u32 character);

/* Get the repeated text input character for this frame (0 if none) */
u32 KeyRepeat_GetTextInput(void);

/* Reset a specific key's repeat state (e.g., when consumed) */
void KeyRepeat_Reset(key_code key);

/* Reset all key repeat states */
void KeyRepeat_ResetAll(void);

#endif /* KEY_REPEAT_H */
