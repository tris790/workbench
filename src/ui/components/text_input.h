#ifndef UI_TEXT_INPUT_H
#define UI_TEXT_INPUT_H

#include "../ui.h"

// Text input helpers
void UI_TextInputPushUndo(ui_text_state *state, const char *text);
b32 UI_TextInputPopUndo(ui_text_state *state, char *buffer, i32 buf_size);
void UI_GetSelectionRange(ui_text_state *state, i32 *out_start, i32 *out_end);
void UI_DeleteSelection(ui_text_state *state, char *buffer, i32 *text_len);

// Text input processing and widget
b32 UI_ProcessTextInput(ui_text_state *state, char *buffer, i32 buffer_size,
                        ui_input *input);
b32 UI_TextInput(char *buffer, i32 buffer_size, const char *placeholder,
                 ui_text_state *state);

#endif
