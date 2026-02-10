/*
 * input.c - Centralized Input Handling System Implementation
 *
 * C99, handmade hero style.
 */

#include "input.h"
#include "key_repeat.h"
#include "ui.h"
#include <string.h>

/* Global input state */
static input_state g_input = {0};

void Input_Init(void) {
  memset(&g_input, 0, sizeof(g_input));
  g_input.focus = WB_INPUT_TARGET_EXPLORER; /* Default focus */
  g_input.focus_stack_depth = 0;
}

input_state *Input_GetState(void) { return &g_input; }

/* ===== Focus Management ===== */

void Input_SetFocus(input_target target) {
  if (target < WB_INPUT_TARGET_COUNT) {
    g_input.focus = target;
  }
}

input_target Input_GetFocus(void) { return g_input.focus; }

void Input_PushFocus(input_target target) {
  /* Push current focus onto stack */
  if (g_input.focus_stack_depth < 4) {
    g_input.focus_stack[g_input.focus_stack_depth] = g_input.focus;
    g_input.focus_stack_depth++;
  }
  g_input.focus = target;
}

void Input_PopFocus(void) {
  /* Pop previous focus from stack */
  if (g_input.focus_stack_depth > 0) {
    g_input.focus_stack_depth--;
    g_input.focus = g_input.focus_stack[g_input.focus_stack_depth];
  } else {
    /* No saved focus, default to explorer */
    g_input.focus = WB_INPUT_TARGET_EXPLORER;
  }
}

b32 Input_HasFocus(input_target target) { return g_input.focus == target; }

/* ===== Frame Lifecycle ===== */

void Input_BeginFrame(void *raw_input_ptr) {
  ui_input *raw_input = (ui_input *)raw_input_ptr;
  /* Reset consumption flags */
  g_input.key_consumed = false;
  g_input.text_consumed = false;
  g_input.mouse_consumed = false;
  g_input.scroll_consumed = false;

  /* Copy raw input */
  g_input.raw.mouse_pos = raw_input->mouse_pos;
  memcpy(g_input.raw.mouse_down, raw_input->mouse_down,
         sizeof(raw_input->mouse_down));
  memcpy(g_input.raw.mouse_pressed, raw_input->mouse_pressed,
         sizeof(raw_input->mouse_pressed));
  memcpy(g_input.raw.mouse_released, raw_input->mouse_released,
         sizeof(raw_input->mouse_released));
  g_input.raw.scroll_delta = raw_input->scroll_delta;
  memcpy(g_input.raw.key_down, raw_input->key_down,
         sizeof(raw_input->key_down));
  memcpy(g_input.raw.key_pressed, raw_input->key_pressed,
         sizeof(raw_input->key_pressed));
  memcpy(g_input.raw.key_released, raw_input->key_released,
         sizeof(raw_input->key_released));
  g_input.raw.modifiers = raw_input->modifiers;
  g_input.raw.text_input = raw_input->text_input;
}

void Input_EndFrame(void) {
  /* Nothing to do currently - could add input recording/playback here */
}

/* ===== Input Consumption ===== */

void Input_ConsumeKeys(void) { g_input.key_consumed = true; }

void Input_ConsumeText(void) { g_input.text_consumed = true; }

void Input_ConsumeMouse(void) { g_input.mouse_consumed = true; }

void Input_ConsumeScroll(void) { g_input.scroll_consumed = true; }

/* ===== Input Queries ===== */

b32 Input_KeyPressed(key_code key) {
  if (g_input.key_consumed)
    return false;
  if (key >= WB_KEY_COUNT)
    return false;
  return g_input.raw.key_pressed[key];
}

b32 Input_KeyPressedRaw(key_code key) {
  if (key >= WB_KEY_COUNT)
    return false;
  return g_input.raw.key_pressed[key];
}

b32 Input_KeyDown(key_code key) {
  if (key >= WB_KEY_COUNT)
    return false;
  return g_input.raw.key_down[key];
}

b32 Input_KeyRepeat(key_code key) {
  if (g_input.key_consumed)
    return false;
  return KeyRepeat_Check(key);
}

u32 Input_GetTextInput(void) {
  if (g_input.text_consumed)
    return 0;
  return g_input.raw.text_input;
}

void Input_SetRepeatedTextInput(u32 text) {
  if (g_input.raw.text_input == 0) {
    g_input.raw.text_input = text;
  }
}

u32 Input_GetModifiers(void) { return g_input.raw.modifiers; }

b32 Input_MousePressed(mouse_button button) {
  if (g_input.mouse_consumed)
    return false;
  if (button >= WB_MOUSE_BUTTON_COUNT)
    return false;
  return g_input.raw.mouse_pressed[button];
}

b32 Input_MouseDown(mouse_button button) {
  if (button >= WB_MOUSE_BUTTON_COUNT)
    return false;
  return g_input.raw.mouse_down[button];
}

b32 Input_MouseReleased(mouse_button button) {
  if (g_input.mouse_consumed)
    return false;
  if (button >= WB_MOUSE_BUTTON_COUNT)
    return false;
  return g_input.raw.mouse_released[button];
}

f32 Input_GetScrollDelta(void) {
  if (g_input.scroll_consumed)
    return 0.0f;
  return g_input.raw.scroll_delta;
}

v2i Input_GetMousePos(void) { return g_input.raw.mouse_pos; }
