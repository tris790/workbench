/*
 * input.h - Centralized Input Handling System
 *
 * Manages input focus and routing to prevent conflicts between components.
 * C99, handmade hero style.
 */

#ifndef INPUT_H
#define INPUT_H

#include "types.h"
#include "platform.h"

/* ===== Input Targets ===== */

typedef enum {
    INPUT_TARGET_NONE = 0,
    INPUT_TARGET_EXPLORER,         /* File explorer panel */
    INPUT_TARGET_TERMINAL,         /* Terminal panel */
    INPUT_TARGET_COMMAND_PALETTE,  /* Command palette (modal) */
    INPUT_TARGET_DIALOG,           /* Modal dialogs (rename, create, delete, etc.) */
    INPUT_TARGET_COUNT
} input_target;

/* ===== Input State ===== */

typedef struct {
    /* Current focused target (receives keyboard input) */
    input_target focus;
    
    /* Focus stack for modal push/pop (max 4 deep should be plenty) */
    input_target focus_stack[4];
    i32 focus_stack_depth;
    
    /* Input consumption flags - set by handlers to prevent double-processing */
    b32 key_consumed;
    b32 text_consumed;
    b32 mouse_consumed;
    b32 scroll_consumed;
    
    /* Raw input from platform */
    struct {
        v2i mouse_pos;
        b32 mouse_down[MOUSE_BUTTON_COUNT];
        b32 mouse_pressed[MOUSE_BUTTON_COUNT];
        b32 mouse_released[MOUSE_BUTTON_COUNT];
        f32 scroll_delta;
        b32 key_down[KEY_COUNT];
        b32 key_pressed[KEY_COUNT];
        b32 key_released[KEY_COUNT];
        u32 modifiers;
        u32 text_input;
    } raw;
} input_state;

/* ===== Global Input State ===== */

/* Initialize input system */
void Input_Init(void);

/* Get the global input state (for direct access if needed) */
input_state* Input_GetState(void);

/* ===== Focus Management ===== */

/* Set input focus to a target (simple - no stack) */
void Input_SetFocus(input_target target);

/* Get current focused target */
input_target Input_GetFocus(void);

/* Push focus (saves current on stack, sets new) - for modals */
void Input_PushFocus(input_target target);

/* Pop focus (restores previous from stack) - when modal closes */
void Input_PopFocus(void);

/* Check if a target currently has focus */
b32 Input_HasFocus(input_target target);

/* ===== Frame Lifecycle ===== */

/* Note: ui_input is defined in ui.h - we need to include it or forward declare. 
   To avoid circular dependencies, the implementation file includes ui.h */

/* Begin input frame - call after collecting platform events */
void Input_BeginFrame(void *raw_input); /* Pass ui_input*, but use void* to avoid circular include */

/* End input frame - cleanup */
void Input_EndFrame(void);

/* ===== Input Consumption ===== */

/* Mark keyboard input as consumed (prevents other handlers from processing) */
void Input_ConsumeKeys(void);

/* Mark text input as consumed */
void Input_ConsumeText(void);

/* Mark mouse input as consumed */
void Input_ConsumeMouse(void);

/* Mark scroll input as consumed */
void Input_ConsumeScroll(void);

/* ===== Input Queries (for handlers) ===== */

/* Check if key was pressed this frame (respects consumption) */
b32 Input_KeyPressed(key_code key);

/* Check if key was pressed (ignores consumption - for global hotkeys) */
b32 Input_KeyPressedRaw(key_code key);

/* Check if key is held down */
b32 Input_KeyDown(key_code key);

/* Get text input codepoint (0 if consumed or none) */
u32 Input_GetTextInput(void);

/* Get current modifiers */
u32 Input_GetModifiers(void);

/* Check if mouse button was pressed (respects consumption) */
b32 Input_MousePressed(mouse_button button);

/* Check if mouse button is down */
b32 Input_MouseDown(mouse_button button);

/* Get scroll delta (respects consumption) */
f32 Input_GetScrollDelta(void);

/* Get mouse position */
v2i Input_GetMousePos(void);

#endif /* INPUT_H */
