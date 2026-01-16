#include "wsh_tokenizer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
static char *strndup(const char *s, size_t n) {
  size_t len = 0;
  while (len < n && s[len])
    len++;
  char *new_str = malloc(len + 1);
  if (!new_str)
    return NULL;
  memcpy(new_str, s, len);
  new_str[len] = '\0';
  return new_str;
}
#endif
void WSH_TokenList_Init(WSH_TokenList *list) {
  list->tokens = NULL;
  list->count = 0;
  list->capacity = 0;
}

void WSH_TokenList_Free(WSH_TokenList *list) {
  for (size_t i = 0; i < list->count; i++) {
    free(list->tokens[i].value);
  }
  free(list->tokens);
  list->tokens = NULL;
  list->count = 0;
  list->capacity = 0;
}

void WSH_TokenList_Add(WSH_TokenList *list, WSH_TokenType type,
                       const char *value, size_t start, size_t len) {
  if (list->count >= list->capacity) {
    list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
    list->tokens = realloc(list->tokens, list->capacity * sizeof(WSH_Token));
  }
  WSH_Token *t = &list->tokens[list->count++];
  t->type = type;
  t->start_pos = start;
  t->len = len;

  // We can copy the substring from the original input using the len
  if (value) {
    t->value = strndup(value, len);
  } else {
    t->value = NULL;
  }
}

static bool is_operator_char(char c) {
  return c == '|' || c == '>' || c == '<' || c == '&' || c == ';';
}

int WSH_Tokenize(const char *input, WSH_TokenList *out_list) {
  WSH_TokenList_Init(out_list);
  const char *p = input;

  while (*p) {
    // Skip whitespace
    while (*p && isspace((unsigned char)*p)) {
      p++;
    }
    if (!*p)
      break;

    size_t start_index = p - input;

    // Check for operators
    if (*p == '|') {
      WSH_TokenList_Add(out_list, WSH_TOKEN_PIPE, p, start_index, 1);
      p++;
    } else if (*p == ';') {
      WSH_TokenList_Add(out_list, WSH_TOKEN_SEMICOLON, p, start_index, 1);
      p++;
    } else if (*p == '&') {
      WSH_TokenList_Add(out_list, WSH_TOKEN_BACKGROUND, p, start_index, 1);
      p++;
    } else if (*p == '<') {
      WSH_TokenList_Add(out_list, WSH_TOKEN_REDIRECT_IN, p, start_index, 1);
      p++;
    } else if (*p == '>') {
      if (*(p + 1) == '>') {
        WSH_TokenList_Add(out_list, WSH_TOKEN_REDIRECT_APP, p, start_index, 2);
        p += 2;
      } else {
        WSH_TokenList_Add(out_list, WSH_TOKEN_REDIRECT_OUT, p, start_index, 1);
        p++;
      }
    } else {
      // Word processing
      const char *start = p;
      while (*p) {
        if (isspace((unsigned char)*p) || is_operator_char(*p)) {
          break;
        }

        if (*p == '\'' || *p == '"') {
          char quote = *p;
          p++;
          while (*p && *p != quote) {
            if (*p == '\\' && *(p + 1)) {
              p += 2; // skip escaped char
            } else {
              p++;
            }
          }
          if (*p == quote) {
            p++;
          } else {
            // Unclosed quote - error
            return WSH_TOKENIZE_ERR_UNCLOSED_QUOTE;
          }
        } else if (*p == '\\') {
          if (*(p + 1))
            p += 2;
          else
            p++;
        } else {
          p++;
        }
      }
      WSH_TokenList_Add(out_list, WSH_TOKEN_WORD, start, start_index,
                        p - start);
    }
  }

  return WSH_TOKENIZE_OK;
}
