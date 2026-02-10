#ifndef UI_TEXT_INPUT_H
#define UI_TEXT_INPUT_H

#include "../ui.h"

/* ===== Text Input Helpers ===== */

void UI_TextInputPushUndo(ui_text_state *state, const char *text);
b32 UI_TextInputPopUndo(ui_text_state *state, char *buffer, i32 buf_size);
void UI_GetSelectionRange(ui_text_state *state, i32 *out_start, i32 *out_end);
void UI_DeleteSelection(ui_text_state *state, char *buffer, i32 *text_len);

/* ===== Text Input Processing and Widget ===== */

b32 UI_ProcessTextInput(ui_text_state *state, char *buffer, i32 buffer_size,
                        ui_input *input);
b32 UI_TextInput(char *buffer, i32 buffer_size, const char *placeholder,
                 ui_text_state *state);

/* ===== Shared Text Rendering Helpers ===== */

/* Measure text width up to a character position (UTF-8 aware).
 * Uses incremental measurement for O(n) total instead of O(n²).
 * Returns the width in pixels. */
i32 UI_MeasureTextWidthUpToPos(font *f, const char *text, i32 char_pos);

/* Find cursor position from click x-coordinate.
 * Uses binary search for O(log n) instead of O(n²).
 * Returns the character position (0 to text_len). */
i32 UI_FindCursorPosFromClick(font *f, const char *text, i32 click_x_offset);

/* Draw selection highlight for text.
 * @param sel_start, sel_end: character positions (not byte offsets) */
void UI_DrawSelectionHighlight(render_context *ctx, font *f, const char *text,
                               i32 sel_start, i32 sel_end, rect bounds,
                               color sel_color);

/* Draw text cursor at character position.
 * @param cursor_pos: character position (not byte offset)
 * @param cursor_height: height of cursor rect (defaults to bounds.h - 6 if 0) */
void UI_DrawTextCursor(render_context *ctx, font *f, const char *text,
                       i32 cursor_pos, rect bounds, color cursor_color,
                       i32 cursor_width, i32 cursor_height);

#endif
