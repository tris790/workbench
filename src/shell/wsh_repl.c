#define _POSIX_C_SOURCE 200809L
#include "wsh_repl.h"
#include "wsh_highlight.h"
#include "wsh_pal.h"
#include "wsh_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <sys/select.h> - Removed for Windows compatibility */
#include <sys/time.h>
#include <unistd.h>

typedef enum { ESC_STATE_NORMAL, ESC_STATE_ESC, ESC_STATE_CSI } EscState;

#define MAX_ARGS 128
#define MAX_CMD_LEN 4096

static void wsh_eval(wsh_state_t *state, char *line) {
  WSH_Job *job = WSH_Parse(line);
  if (!job)
    return;

  WSH_Pipeline *pl = job->pipelines;
  while (pl) {
    WSH_Command *cmd = pl->commands;
    if (!cmd || cmd->argc == 0) {
      pl = pl->next;
      continue;
    }

    char **argv = cmd->args;
    int argc = cmd->argc;

    /* Builtins */
    if (strcmp(argv[0], "exit") == 0) {
      state->running = false;
    } else if (strcmp(argv[0], "cd") == 0) {
      if (argc > 1) {
        WSH_PAL_Chdir(state, argv[1]);
      } else {
        const char *home = WSH_State_GetEnv(state, "HOME");
        if (home)
          WSH_PAL_Chdir(state, home);
      }
    } else if (strcmp(argv[0], "pwd") == 0) {
      printf("%s\n", state->cwd);
    } else if (strcmp(argv[0], "set") == 0) {
      if (argc >= 4 && strcmp(argv[1], "-U") == 0) {
        WSH_State_SetEnv(state, argv[2], argv[3]);
      } else if (argc >= 3) {
        WSH_State_SetEnv(state, argv[1], argv[2]);
      }
    } else if (strcmp(argv[0], "abbr") == 0) {
      if (argc >= 4 && strcmp(argv[1], "-a") == 0) {
        WSH_Abbr_Add(&state->abbreviations, argv[2], argv[3]);
      }
    } else if (strcmp(argv[0], "export") == 0) {
      if (argc > 1) {
        char *eq = strchr(argv[1], '=');
        if (eq) {
          *eq = '\0';
          WSH_State_SetEnv(state, argv[1], eq + 1);
        }
      }
    } else if (strcmp(argv[0], "pbcopy") == 0) {
      if (argc > 1) {
        size_t len = 0;
        for (int i = 1; i < argc; i++)
          len += strlen(argv[i]) + 1;
        char *buf = malloc(len + 1);
        buf[0] = '\0';
        for (int i = 1; i < argc; i++) {
          strcat(buf, argv[i]);
          if (i < argc - 1)
            strcat(buf, " ");
        }
        WSH_PAL_ClipboardCopy(buf, len - 1);
        free(buf);
      } else {
        WSH_PAL_ClipboardCopyFromStdin();
      }
    } else if (strcmp(argv[0], "pbpaste") == 0) {
      char *text = WSH_PAL_ClipboardPaste();
      if (text) {
        printf("%s", text);
        free(text);
      }
    } else {
      /* For now, just execute the first command in the pipeline if it's not a
         builtin. Full pipe/redirect support will require more PAL work. */
      state->last_exit_code = WSH_PAL_Execute(state, argv);
    }

    pl = pl->next;
  }

  WSH_Job_Free(job);
}

typedef struct {
  char buf[MAX_CMD_LEN];
  int len;
  int pos;
} InputBuffer;

static void buffer_insert(InputBuffer *ib, char c) {
  if (ib->len >= MAX_CMD_LEN - 1)
    return;
  memmove(ib->buf + ib->pos + 1, ib->buf + ib->pos, ib->len - ib->pos);
  ib->buf[ib->pos] = c;
  ib->len++;
  ib->pos++;
  ib->buf[ib->len] = '\0';
}

static void buffer_delete_back(InputBuffer *ib) {
  if (ib->pos > 0) {
    memmove(ib->buf + ib->pos - 1, ib->buf + ib->pos, ib->len - ib->pos);
    ib->pos--;
    ib->len--;
    ib->buf[ib->len] = '\0';
  }
}

static void buffer_kill_word(InputBuffer *ib) {
  while (ib->pos > 0 && isspace(ib->buf[ib->pos - 1])) {
    buffer_delete_back(ib);
  }
  while (ib->pos > 0 && !isspace(ib->buf[ib->pos - 1])) {
    buffer_delete_back(ib);
  }
}

static void buffer_move_home(InputBuffer *ib) { ib->pos = 0; }
static void buffer_move_end(InputBuffer *ib) { ib->pos = ib->len; }
static void buffer_kill_line(InputBuffer *ib) {
  ib->buf[0] = '\0';
  ib->len = 0;
  ib->pos = 0;
}
static void buffer_kill_to_end(InputBuffer *ib) {
  ib->len = ib->pos;
  ib->buf[ib->len] = '\0';
}

static void buffer_delete_at(InputBuffer *ib) {
  if (ib->pos < ib->len) {
    memmove(ib->buf + ib->pos, ib->buf + ib->pos + 1, ib->len - ib->pos);
    ib->len--;
    ib->buf[ib->len] = '\0';
  }
}

static void print_prompt(wsh_state_t *state) {
  char *user = getenv("USER");
  if (!user)
    user = "user";

  const char *cwd = state->cwd;
  const char *home = WSH_State_GetEnv(state, "HOME");

  printf("\r\033[1;32m%s\033[0m:", user);

  if (home && strncmp(cwd, home, strlen(home)) == 0) {
    printf("\033[1;34m~%s\033[0m> ", cwd + strlen(home));
  } else {
    printf("\033[1;34m%s\033[0m> ", cwd);
  }
}

static int get_prompt_len(wsh_state_t *state) {
  char *user = getenv("USER");
  if (!user)
    user = "user";

  const char *cwd = state->cwd;
  const char *home = WSH_State_GetEnv(state, "HOME");

  int len = strlen(user) + 1; /* user: */

  if (home && strncmp(cwd, home, strlen(home)) == 0) {
    len += 1 + strlen(cwd + strlen(home)) + 2; /* ~path>  */
  } else {
    len += strlen(cwd) + 2; /* path>  */
  }
  return len;
}

static void draw_pager(wsh_state_t *state, InputBuffer *ib) {
  wsh_pager_t *p = &state->pager;
  if (!p->active || p->count == 0)
    return;

  int max_show = 5;
  int start = 0;
  if (p->selected_index > max_show - 1) {
    start = p->selected_index - (max_show - 1);
  }
  int end = start + max_show;
  if (end > p->count)
    end = p->count;

  int lines_printed = 0;
  /* Move to next line */
  printf("\r\n");
  lines_printed++;

  for (int i = start; i < end; i++) {
    wsh_completion_t *c = &p->candidates[i];
    int idx = i;

    if (idx == p->selected_index) {
      printf("\033[7m"); /* Invert */
    }

    printf(" %s ", c->display);
    if (c->description) {
      printf("  \033[90m%s\033[0m", c->description);
    }

    if (idx == p->selected_index) {
      printf("\033[27m");
    }
    printf("\033[K\r\n");
    lines_printed++;
  }

  /* Move back up to prompt line */
  printf("\033[%dA", lines_printed);
  /* Restore horizontal position */
  int prompt_len = get_prompt_len(state);
  int col = prompt_len + ib->pos + 1;
  printf("\033[%dG", col);
}

static void handle_abbr(wsh_state_t *state, InputBuffer *ib) {
  if (ib->pos == 0)
    return;
  /* Only expand if we are at end of word or just typed a delimiter */
  /* But this is called BEFORE inserting the space/enter usually? */
  /* If called on Space, we are adding a space. logic: check word before cursor.
   */

  int end = ib->pos;
  int start = end - 1;
  while (start >= 0 && !isspace(ib->buf[start]))
    start--;
  start++;

  if (start >= end)
    return;

  char word[256];
  int len = end - start;
  if (len >= 256)
    len = 255;
  strncpy(word, ib->buf + start, len);
  word[len] = '\0';

  const char *expansion = WSH_Abbr_Expand(&state->abbreviations, word);
  if (expansion) {
    int exp_len = strlen(expansion);
    int diff = exp_len - len;

    if (ib->len + diff >= MAX_CMD_LEN - 1)
      return;

    memmove(ib->buf + start + exp_len, ib->buf + end, ib->len - end + 1);
    memcpy(ib->buf + start, expansion, exp_len);
    ib->len += diff;
    ib->pos += diff;
  }
}

static void refresh_line(wsh_state_t *state, InputBuffer *ib) {
  /* Move cursor to left edge */
  printf("\r");
  printf("\033[J");
  /* Print prompt */
  print_prompt(state);
  /* Print highlighted buffer */
  char *hl = WSH_Highlight(state, ib->buf);
  printf("%s", hl);
  free(hl);

  /* Suggestion */
  if (ib->len > 0) {
    const char *sugg = WSH_History_GetSuggestion(&state->history, ib->buf);
    if (sugg && strncmp(sugg, ib->buf, ib->len) == 0) {
      /* Print remainder in grey */
      printf("\033[90m%s\033[0m", sugg + ib->len);
    }
  }

  /* Clear to end of line */
  printf("\033[K");

  /* Move cursor to correct position */
  int prompt_len = get_prompt_len(state);
  int col = prompt_len + ib->pos + 1; /* 1-based column */
  printf("\033[%dG", col);

  if (state->pager.active) {
    draw_pager(state, ib);
  }

  fflush(stdout);
}

static void handle_history_search(wsh_state_t *state, InputBuffer *ib,
                                  int *h_idx, char *prefix, int dir) {
  wsh_history_t *hist = &state->history;
  if (hist->count == 0)
    return;

  /* If starting search, save current buffer as prefix */
  if (*h_idx == -1) {
    if (dir == 1)
      return; /* Can't go down if not in history */
    strncpy(prefix, ib->buf, MAX_CMD_LEN - 1);
    *h_idx = hist->count;
  }

  int start = *h_idx + (dir == -1 ? -1 : 1);
  int found_idx = -1;
  size_t prefix_len = strlen(prefix);

  if (dir == -1) { /* UP */
    for (int i = start; i >= 0; i--) {
      if (strncmp(hist->entries[i].cmd, prefix, prefix_len) == 0) {
        found_idx = i;
        break;
      }
    }
  } else { /* DOWN */
    for (int i = start; i < (int)hist->count; i++) {
      if (strncmp(hist->entries[i].cmd, prefix, prefix_len) == 0) {
        found_idx = i;
        break;
      }
    }
  }

  if (found_idx != -1) {
    *h_idx = found_idx;
    strncpy(ib->buf, hist->entries[*h_idx].cmd, MAX_CMD_LEN - 1);
    ib->len = strlen(ib->buf);
    ib->pos = ib->len;
  } else if (dir == 1) {
    /* If went down past end, restore original */
    if (start >= (int)hist->count) {
      *h_idx = -1;
      strncpy(ib->buf, prefix, MAX_CMD_LEN - 1);
      ib->len = strlen(ib->buf);
      ib->pos = ib->len;
    }
  }
}

static void accept_suggestion(wsh_state_t *state, InputBuffer *ib) {
  if (ib->len == 0)
    return;
  const char *sugg = WSH_History_GetSuggestion(&state->history, ib->buf);
  if (sugg && strncmp(sugg, ib->buf, ib->len) == 0) {
    strncpy(ib->buf, sugg, MAX_CMD_LEN - 1);
    ib->len = strlen(ib->buf);
    ib->pos = ib->len;
  }
}

void WSH_REPL_Run(wsh_state_t *state) {
  printf("\033[1;36mWSH Shell 0.1\033[0m\n");

  WSH_PAL_EnableRawMode();

  InputBuffer ib = {0};
  int history_idx = -1;
  char history_search_prefix[MAX_CMD_LEN] = {0};

  b32 needs_refresh = true;
  EscState esc_state = ESC_STATE_NORMAL;
  char csi_param = 0;

  while (state->running) {
    if (needs_refresh) {
      refresh_line(state, &ib);
      needs_refresh = false;
    }

    int sel = WSH_PAL_SelectStdin(50000); /* 50ms timeout */
    if (sel < 0)
      break;
    if (sel == 0) {
      /* Background updates can go here */
      continue;
    }

    char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
      continue;

    needs_refresh = true;

    /* Handle Escape Sequences */
    if (esc_state == ESC_STATE_ESC) {
      if (c == '[') {
        esc_state = ESC_STATE_CSI;
        csi_param = 0;
        continue;
      } else if (c == '\r' || c == '\n') { /* Alt+Enter */
        buffer_insert(&ib, '\n');
        history_idx = -1;
        esc_state = ESC_STATE_NORMAL;
        continue;
      } else {
        /* Unrecognized Alt sequence or standalone ESC */
        if (state->pager.active)
          WSH_Pager_Clear(&state->pager);
        esc_state = ESC_STATE_NORMAL;
        /* fall through to handle 'c' as a normal key if it's not and ESC */
        if (c == '\x1b') {
          esc_state = ESC_STATE_ESC;
          continue;
        }
      }
    } else if (esc_state == ESC_STATE_CSI) {
      if (isdigit((unsigned char)c)) {
        csi_param = c;
        continue;
      }

      switch (c) {
      case 'A': /* Up */
        if (state->pager.active) {
          state->pager.selected_index--;
          if (state->pager.selected_index < 0)
            state->pager.selected_index = state->pager.count - 1;
        } else {
          handle_history_search(state, &ib, &history_idx, history_search_prefix,
                                -1);
        }
        break;
      case 'B': /* Down */
        if (state->pager.active) {
          state->pager.selected_index++;
          if (state->pager.selected_index >= state->pager.count)
            state->pager.selected_index = 0;
        } else {
          handle_history_search(state, &ib, &history_idx, history_search_prefix,
                                1);
        }
        break;
      case 'C': /* Right */
        if (state->pager.active)
          WSH_Pager_Clear(&state->pager);
        if (ib.pos < ib.len)
          ib.pos++;
        else
          accept_suggestion(state, &ib);
        break;
      case 'D': /* Left */
        if (state->pager.active)
          WSH_Pager_Clear(&state->pager);
        if (ib.pos > 0)
          ib.pos--;
        break;
      case 'Z': /* Shift Tab */
        if (state->pager.active) {
          state->pager.selected_index--;
          if (state->pager.selected_index < 0)
            state->pager.selected_index = state->pager.count - 1;
        }
        break;
      case '~':
        if (csi_param == '3') { /* Delete */
          buffer_delete_at(&ib);
          if (state->pager.active)
            WSH_Pager_Clear(&state->pager);
          history_idx = -1;
        }
        break;
      }
      esc_state = ESC_STATE_NORMAL;
      continue;
    }

    if (c == '\x1b') {
      esc_state = ESC_STATE_ESC;
      continue;
    }

    switch (c) {
    case '\t': /* Tab */
      if (!state->pager.active) {
        WSH_Complete(&state->pager, ib.buf, ib.pos, state->cwd);
      } else {
        state->pager.selected_index++;
        if (state->pager.selected_index >= state->pager.count)
          state->pager.selected_index = 0;
      }
      break;
    case 1: /* Ctrl+A */
      buffer_move_home(&ib);
      break;
    case 5: /* Ctrl+E */
      buffer_move_end(&ib);
      break;
    case 2: /* Ctrl+B */
      if (ib.pos > 0)
        ib.pos--;
      break;
    case 6: /* Ctrl+F */
      if (ib.pos < ib.len)
        ib.pos++;
      else
        accept_suggestion(state, &ib);
      break;
    case 23: /* Ctrl+W */
      buffer_kill_word(&ib);
      history_idx = -1;
      if (state->pager.active)
        WSH_Pager_Clear(&state->pager);
      break;
    case 21: /* Ctrl+U */
      buffer_kill_line(&ib);
      history_idx = -1;
      if (state->pager.active)
        WSH_Pager_Clear(&state->pager);
      break;
    case 11: /* Ctrl+K */
      buffer_kill_to_end(&ib);
      history_idx = -1;
      if (state->pager.active)
        WSH_Pager_Clear(&state->pager);
      break;
    case 12: /* Ctrl+L */
      printf("\033[2J\033[H");
      refresh_line(state, &ib);
      break;
    case 3: /* Ctrl+C */
      printf("^C\n");
      ib.len = 0;
      ib.pos = 0;
      ib.buf[0] = '\0';
      history_idx = -1;
      WSH_Pager_Clear(&state->pager);
      break;
    case 4: /* Ctrl+D */
      if (state->pager.active) {
        WSH_Pager_Clear(&state->pager);
      } else if (ib.len == 0) {
        state->running = false;
      } else {
        buffer_delete_at(&ib);
        history_idx = -1;
      }
      break;
    case 127: /* Backspace */
    case 8:
      buffer_delete_back(&ib);
      history_idx = -1;
      if (state->pager.active) {
        /* Update filter? For now just close or re-complete */
        WSH_Complete(&state->pager, ib.buf, ib.pos, state->cwd);
        if (state->pager.count == 0)
          WSH_Pager_Clear(&state->pager);
      }
      break;
    case ' ': /* Space */
      handle_abbr(state, &ib);
      buffer_insert(&ib, ' ');
      if (state->pager.active)
        WSH_Pager_Clear(&state->pager);
      history_idx = -1;
      break;
    case '\r':
    case '\n':
      if (state->pager.active) {
        wsh_completion_t *comp =
            &state->pager.candidates[state->pager.selected_index];
        /* Replace filter */
        int flen = state->pager.filter_len;
        for (int i = 0; i < flen; i++)
          buffer_delete_back(&ib);
        for (char *p = comp->value; *p; p++)
          buffer_insert(&ib, *p);

        WSH_Pager_Clear(&state->pager);
      } else {
        printf("\r\n");
        if (ib.len > 0) {
          handle_abbr(state, &ib); /* Allow abbr on enter too */
          WSH_History_Add(&state->history, ib.buf);
          wsh_eval(state, ib.buf);
          ib.len = 0;
          ib.pos = 0;
          ib.buf[0] = '\0';
          history_idx = -1;
        }
      }
      break;
    default:
      if (!iscntrl(c)) {
        buffer_insert(&ib, c);
        history_idx = -1;
        if (state->pager.active) {
          /* Update filter */
          WSH_Complete(&state->pager, ib.buf, ib.pos, state->cwd);
          if (state->pager.count == 0)
            state->pager.active = false;
        }
      }
      break;
    }
  }

  WSH_PAL_DisableRawMode();
}
