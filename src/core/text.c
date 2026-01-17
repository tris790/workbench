/*
 * text.c - Text processing utilities implementation
 *
 * C99, handmade hero style.
 */

#include "text.h"

#include <string.h>

/* Internal: Find line break point for text wrapping.
 * Returns the index where this line should end (exclusive).
 */
static i32 FindLineBreak(const char *text, i32 len, font *f, i32 max_width) {
  if (len == 0)
    return 0;

  i32 last_space = -1;
  i32 current_width = 0;

  for (i32 i = 0; i < len; i++) {
    char c = text[i];
    char temp[2] = {c, '\0'};
    i32 char_w = Font_MeasureWidth(f, temp);

    if (current_width + char_w > max_width) {
      /* Exceeded width - break at last space or force break here */
      if (last_space > 0) {
        return last_space;
      }
      /* No space found - force break (at least 1 char) */
      return i > 0 ? i : 1;
    }

    current_width += char_w;

    if (c == ' ') {
      last_space = i;
    }
  }

  /* Entire remaining text fits */
  return len;
}

/* Internal: Count lines needed for wrapping (pass 1) */
static i32 CountWrappedLines(const char *text, font *f, i32 max_width) {
  if (!text || !f || max_width <= 0)
    return 0;

  i32 len = (i32)strlen(text);
  if (len == 0)
    return 0;

  i32 line_count = 0;
  i32 pos = 0;

  while (pos < len) {
    i32 remaining = len - pos;
    i32 line_len = FindLineBreak(text + pos, remaining, f, max_width);

    if (line_len == 0)
      break; /* Safety: prevent infinite loop */

    line_count++;
    pos += line_len;

    /* Skip space at break point */
    if (pos < len && text[pos] == ' ') {
      pos++;
    }
  }

  return line_count > 0 ? line_count : 1;
}

wrapped_text Text_Wrap(memory_arena *arena, const char *text, font *f,
                       i32 max_width) {
  wrapped_text result = {0};

  if (!arena || !text || !f || max_width <= 0) {
    return result;
  }

  i32 len = (i32)strlen(text);
  if (len == 0) {
    return result;
  }

  /* Pass 1: Count lines needed */
  i32 line_count = CountWrappedLines(text, f, max_width);
  if (line_count == 0) {
    return result;
  }

  /* Pass 2: Allocate and fill lines */
  char **lines = ArenaPushArray(arena, char *, line_count);
  if (!lines) {
    return result;
  }

  i32 pos = 0;
  i32 line_idx = 0;

  while (pos < len && line_idx < line_count) {
    i32 remaining = len - pos;
    i32 line_len = FindLineBreak(text + pos, remaining, f, max_width);

    if (line_len == 0)
      break;

    /* Allocate line string from arena (+1 for null terminator) */
    char *line = ArenaPushArray(arena, char, line_len + 1);
    if (!line) {
      return result; /* Arena exhausted */
    }

    memcpy(line, text + pos, line_len);
    line[line_len] = '\0';

    lines[line_idx++] = line;
    pos += line_len;

    /* Skip space at break point */
    if (pos < len && text[pos] == ' ') {
      pos++;
    }
  }

  result.lines = lines;
  result.count = line_idx;
  return result;
}

i32 Text_GetWrappedHeight(i32 line_count, font *f) {
  if (!f || line_count <= 0)
    return 0;
  return line_count * Font_GetLineHeight(f);
}

i32 Text_UTF8Length(const char *str) {
  i32 len = 0;
  while (*str) {
    if ((*str & 0xC0) != 0x80)
      len++;
    str++;
  }
  return len;
}

i32 Text_UTF8ByteOffset(const char *str, i32 char_index) {
  i32 byte = 0;
  i32 ch = 0;
  while (str[byte] && ch < char_index) {
    if ((str[byte] & 0xC0) != 0x80)
      ch++;
    byte++;
  }
  return byte;
}

static b32 IsSeparator(char c) { return c == ' ' || c == '/'; }

i32 Text_FindWordBoundaryLeft(const char *text, i32 start_pos) {
  if (start_pos <= 0)
    return 0;

  i32 cursor = start_pos - 1;

  /* Skip separators if we started on one */
  while (cursor >= 0 && IsSeparator(text[Text_UTF8ByteOffset(text, cursor)])) {
    cursor--;
  }

  /* Skip non-separators */
  while (cursor >= 0 && !IsSeparator(text[Text_UTF8ByteOffset(text, cursor)])) {
    cursor--;
  }

  return cursor + 1;
}

i32 Text_FindWordBoundaryRight(const char *text, i32 start_pos) {
  i32 char_count = Text_UTF8Length(text);
  if (start_pos >= char_count)
    return char_count;

  i32 cursor = start_pos;

  /* Skip non-separators forward */
  while (cursor < char_count &&
         !IsSeparator(text[Text_UTF8ByteOffset(text, cursor)])) {
    cursor++;
  }

  /* Skip separators forward */
  while (cursor < char_count &&
         IsSeparator(text[Text_UTF8ByteOffset(text, cursor)])) {
    cursor++;
  }

  return cursor;
}
