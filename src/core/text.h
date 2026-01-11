/*
 * text.h - Text processing utilities
 *
 * C99, handmade hero style.
 */

#ifndef TEXT_H
#define TEXT_H

#include "../renderer/font.h"
#include "types.h"

/* Result of text wrapping - array of string pointers + count */
typedef struct {
  char **lines;
  i32 count;
} wrapped_text;

/* Wrap text into multiple lines ensuring no line exceeds max_width.
 * Uses arena allocation - no manual freeing required.
 *
 * Algorithm:
 *   Pass 1: Count lines needed (no allocation)
 *   Pass 2: Allocate exact size from arena, fill lines
 *
 * @param arena Memory arena to allocate from.
 * @param text The source text to wrap.
 * @param f The font used for measuring width.
 * @param max_width The maximum width in pixels for a single line.
 * @return wrapped_text with lines array and count. lines is NULL on failure.
 */
wrapped_text Text_Wrap(memory_arena *arena, const char *text, font *f,
                       i32 max_width);

/* Calculate the height required to render the wrapped text.
 *
 * @param line_count The number of lines.
 * @param f The font (to get line height).
 * @return Total height in pixels.
 */
i32 Text_GetWrappedHeight(i32 line_count, font *f);

#endif /* TEXT_H */
