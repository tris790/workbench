#include "wsh_highlight.h"
#include "wsh_pal.h"
#include "wsh_tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COLOR_RESET "\033[0m"
#define COLOR_VALID_CMD "\033[1;34m" // Blue
#define COLOR_INVALID_CMD "\033[31m" // Red
#define COLOR_KEYWORD "\033[1;35m"   // Magenta
#define COLOR_STRING "\033[32m"      // Green
#define COLOR_OPTION "\033[36m"      // Cyan
#define COLOR_VAR "\033[35m"         // Magenta
#define COLOR_PATH "\033[4m"         // Underline

static bool is_builtin(const char *cmd) {
  const char *builtins[] = {"cd",    "exit",    "export", "pwd",
                            "alias", "history", NULL};
  for (int i = 0; builtins[i]; i++) {
    if (strcmp(cmd, builtins[i]) == 0)
      return true;
  }
  return false;
}

static bool is_keyword(const char *cmd) {
  const char *keywords[] = {"if",    "then",     "else",   "end",  "for",
                            "while", "function", "switch", "case", NULL};
  for (int i = 0; keywords[i]; i++) {
    if (strcmp(cmd, keywords[i]) == 0)
      return true;
  }
  return false;
}

static bool check_path_executable(wsh_state_t *state, const char *cmd) {
  if (strchr(cmd, '/')) {
    return WSH_PAL_IsExecutable(cmd);
  }

  char *path_env = NULL;
  const char *env_val = WSH_State_GetEnv(state, "PATH");
  if (!env_val)
    return false;

  path_env = strdup(env_val);
  char *saveptr;
  const char *sep = WSH_PAL_GetPathSeparator();
  char *dir = strtok_r(path_env, sep, &saveptr);
  bool found = false;

  char full_path[4096];

  while (dir) {
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
    if (WSH_PAL_IsExecutable(full_path)) {
      found = true;
      break;
    }
    dir = strtok_r(NULL, sep, &saveptr);
  }

  free(path_env);
  return found;
}

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} wsh_buf_t;

static void buf_init(wsh_buf_t *b) {
  b->cap = 256;
  b->buf = malloc(b->cap);
  b->buf[0] = '\0';
  b->len = 0;
}

static void buf_append_n(wsh_buf_t *b, const char *s, size_t n) {
  if (b->len + n >= b->cap) {
    b->cap = (b->len + n + 1) * 2;
    b->buf = realloc(b->buf, b->cap);
  }
  memcpy(b->buf + b->len, s, n);
  b->len += n;
  b->buf[b->len] = '\0';
}

static void buf_append(wsh_buf_t *b, const char *s) {
  buf_append_n(b, s, strlen(s));
}

char *WSH_Highlight(wsh_state_t *state, const char *input) {
  WSH_TokenList tokens;
  WSH_TokenList_Init(&tokens);
  WSH_Tokenize(input, &tokens);

  size_t input_len = strlen(input);
  wsh_buf_t out;
  buf_init(&out);

  size_t cursor = 0;
  bool expect_command = true;

  for (size_t i = 0; i < tokens.count; i++) {
    WSH_Token *t = &tokens.tokens[i];

    // Append gap
    if (t->start_pos > cursor) {
      buf_append_n(&out, input + cursor, t->start_pos - cursor);
    }

    const char *color = NULL; // NULL means default/no extra color

    if (t->type == WSH_TOKEN_WORD) {
      bool quoted = (t->value[0] == '"' || t->value[0] == '\'');

      // Clean value for checking
      char *clean_val = strdup(t->value);
      if (quoted && strlen(clean_val) >= 2) {
        // Remove quotes for check
        clean_val[strlen(clean_val) - 1] = '\0';
        memmove(clean_val, clean_val + 1, strlen(clean_val));
      }

      if (expect_command) {
        if (is_keyword(clean_val)) {
          color = COLOR_KEYWORD;
        } else if (is_builtin(clean_val)) {
          color = COLOR_VALID_CMD;
        } else {
          if (check_path_executable(state, clean_val)) {
            color = COLOR_VALID_CMD;
          } else {
            color = COLOR_INVALID_CMD;
          }
        }
        expect_command = false;
      } else {
        // Argument
        if (t->value[0] == '-') {
          color = COLOR_OPTION;
        } else if (quoted) {
          color = COLOR_STRING;
        } else if (t->value[0] == '$') {
          color = COLOR_VAR;
        } else {
          if (WSH_PAL_IsExecutable(clean_val) || access(clean_val, F_OK) == 0) {
            color = COLOR_PATH;
          }
        }
      }
      free(clean_val);
    } else {
      // Operators
      if (t->type == WSH_TOKEN_PIPE || t->type == WSH_TOKEN_SEMICOLON ||
          t->type == WSH_TOKEN_BACKGROUND) {
        expect_command = true;
      }
    }

    if (color) {
      buf_append(&out, color);
      buf_append_n(&out, input + t->start_pos, t->len);
      buf_append(&out, COLOR_RESET);
    } else {
      buf_append_n(&out, input + t->start_pos, t->len);
    }

    cursor = t->start_pos + t->len;
  }

  // Append remainder
  if (cursor < input_len) {
    buf_append(&out, input + cursor);
  }

  WSH_TokenList_Free(&tokens);
  return out.buf;
}
