#include "wsh_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  WSH_TokenList *tokens;
  size_t pos;
} WSH_Parser;

static WSH_Token *peek(WSH_Parser *p) {
  if (p->pos < p->tokens->count)
    return &p->tokens->tokens[p->pos];
  return NULL;
}

static WSH_Token *advance(WSH_Parser *p) {
  WSH_Token *t = peek(p);
  if (t)
    p->pos++;
  return t;
}

static bool check(WSH_Parser *p, WSH_TokenType type) {
  WSH_Token *t = peek(p);
  return t && t->type == type;
}

static char *unquote_word(WSH_Token *t) {
  if (!t || !t->value)
    return NULL;

  const char *v = t->value;
  size_t len = strlen(v);
  char *cleaned = malloc(len + 1);
  if (!cleaned)
    return NULL;

  char *dest = cleaned;
  bool in_single = false;
  bool in_double = false;

  for (size_t i = 0; v[i]; i++) {
    if (v[i] == '\\' && !in_single) {
      if (v[i + 1]) {
        *dest++ = v[++i];
      } else {
        // Trailing backslash, keep it?
        *dest++ = v[i];
      }
    } else if (v[i] == '\'' && !in_double) {
      in_single = !in_single;
    } else if (v[i] == '"' && !in_single) {
      in_double = !in_double;
    } else {
      *dest++ = v[i];
    }
  }
  *dest = '\0';
  return cleaned;
}

static WSH_Redirect *parse_redirects(WSH_Parser *p) {
  WSH_Redirect *head = NULL;
  WSH_Redirect *tail = NULL;

  while (check(p, WSH_TOKEN_REDIRECT_IN) || check(p, WSH_TOKEN_REDIRECT_OUT) ||
         check(p, WSH_TOKEN_REDIRECT_APP)) {
    WSH_Token *op = advance(p);
    WSH_Redirect *r = calloc(1, sizeof(WSH_Redirect));
    if (!r)
      break;

    if (op->type == WSH_TOKEN_REDIRECT_OUT)
      r->mode = 0;
    else if (op->type == WSH_TOKEN_REDIRECT_APP)
      r->mode = 1;
    else
      r->mode = 2;

    if (check(p, WSH_TOKEN_WORD)) {
      r->filename = unquote_word(advance(p));
      if (!r->filename) {
        free(r);
        break;
      }
    } else {
      // Error: Missing filename
      free(r);
      break;
    }

    if (tail) {
      tail->next = r;
      tail = r;
    } else {
      head = tail = r;
    }
  }
  return head;
}

static WSH_Command *parse_command(WSH_Parser *p) {
  if (!check(p, WSH_TOKEN_WORD))
    return NULL;

  WSH_Command *cmd = calloc(1, sizeof(WSH_Command));
  if (!cmd)
    return NULL;

  int capacity = 4;
  cmd->args = malloc(capacity * sizeof(char *));
  if (!cmd->args) {
    free(cmd);
    return NULL;
  }

  while (check(p, WSH_TOKEN_WORD) || check(p, WSH_TOKEN_REDIRECT_IN) ||
         check(p, WSH_TOKEN_REDIRECT_OUT) || check(p, WSH_TOKEN_REDIRECT_APP)) {
    if (check(p, WSH_TOKEN_WORD)) {
      if (cmd->argc >= capacity - 1) { // Leave space for NULL
        capacity *= 2;
        char **new_args = realloc(cmd->args, capacity * sizeof(char *));
        if (!new_args)
          break;
        cmd->args = new_args;
      }
      char *arg = unquote_word(advance(p));
      if (arg) {
        cmd->args[cmd->argc++] = arg;
      }
    } else {
      // Append redirects to list
      WSH_Redirect *r = parse_redirects(p);
      if (r) {
        if (!cmd->redirects) {
          cmd->redirects = r;
        } else {
          WSH_Redirect *last = cmd->redirects;
          while (last->next)
            last = last->next;
          last->next = r;
        }
      }
    }
  }

  // Finalize args with NULL
  char **final_args = realloc(cmd->args, (cmd->argc + 1) * sizeof(char *));
  if (final_args) {
    cmd->args = final_args;
  }
  cmd->args[cmd->argc] = NULL;

  return cmd;
}

static WSH_Pipeline *parse_pipeline(WSH_Parser *p) {
  WSH_Command *head_cmd = parse_command(p);
  if (!head_cmd)
    return NULL;

  WSH_Pipeline *pl = calloc(1, sizeof(WSH_Pipeline));
  if (!pl) {
    // Should free head_cmd here but we have free functions later.
    // However, it's better to be clean.
    return NULL;
  }
  pl->commands = head_cmd;
  WSH_Command *curr = head_cmd;

  while (check(p, WSH_TOKEN_PIPE)) {
    advance(p); // eat |
    WSH_Command *next = parse_command(p);
    if (next) {
      curr->next = next;
      curr = next;
    } else {
      // Error: Pipe without command
      break;
    }
  }

  if (check(p, WSH_TOKEN_BACKGROUND)) {
    advance(p);
    pl->background = true;
  }

  return pl;
}

WSH_Job *WSH_Parse(const char *input) {
  if (!input || !*input)
    return NULL;

  WSH_TokenList tokens;
  if (WSH_Tokenize(input, &tokens) != WSH_TOKENIZE_OK) {
    WSH_TokenList_Free(&tokens);
    return NULL;
  }

  WSH_Parser p = {&tokens, 0};

  WSH_Job *job = calloc(1, sizeof(WSH_Job));
  if (!job) {
    WSH_TokenList_Free(&tokens);
    return NULL;
  }

  WSH_Pipeline *head_pl = NULL;
  WSH_Pipeline *tail_pl = NULL;

  while (peek(&p)) {
    if (check(&p, WSH_TOKEN_SEMICOLON)) {
      advance(&p);
      continue;
    }

    WSH_Pipeline *pl = parse_pipeline(&p);
    if (!pl) {
      if (peek(&p))
        advance(&p);
      continue;
    }

    if (tail_pl) {
      tail_pl->next = pl;
      tail_pl = pl;
    } else {
      head_pl = tail_pl = pl;
    }
  }

  job->pipelines = head_pl;

  WSH_TokenList_Free(&tokens);
  return job;
}

static void free_cmd(WSH_Command *cmd) {
  while (cmd) {
    for (int i = 0; i < cmd->argc; i++)
      free(cmd->args[i]);
    free(cmd->args);

    WSH_Redirect *r = cmd->redirects;
    while (r) {
      WSH_Redirect *next = r->next;
      free(r->filename);
      free(r);
      r = next;
    }

    WSH_Command *next = cmd->next;
    free(cmd);
    cmd = next;
  }
}

void WSH_Job_Free(WSH_Job *job) {
  if (!job)
    return;
  WSH_Pipeline *pl = job->pipelines;
  while (pl) {
    free_cmd(pl->commands);
    WSH_Pipeline *next = pl->next;
    free(pl);
    pl = next;
  }
  free(job);
}
