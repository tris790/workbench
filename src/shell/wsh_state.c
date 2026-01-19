#include "wsh_state.h"
#include "wsh_pal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

wsh_state_t *WSH_State_Create(int argc, char **argv, char **envp) {
  (void)argc;
  (void)argv;

  wsh_state_t *state = (wsh_state_t *)malloc(sizeof(wsh_state_t));
  if (!state)
    return NULL;

  memset(state, 0, sizeof(wsh_state_t));
  state->running = true;
  state->rows = 24;
  state->cols = 80;

  /* Initialize environment */
  state->env_capacity = 64;
  state->env_vars =
      (wsh_env_var_t *)malloc(sizeof(wsh_env_var_t) * state->env_capacity);

  if (envp) {
    for (char **env = envp; *env != 0; env++) {
      char *entry = strdup(*env);
      char *eq = strchr(entry, '=');
      if (eq) {
        *eq = '\0';
        WSH_State_SetEnv(state, entry, eq + 1);
      }
      free(entry);
    }
  }

  WSH_State_UpdateCWD(state);

  WSH_History_Init(&state->history);
  WSH_History_Load(&state->history);

  WSH_Pager_Init(&state->pager);
  WSH_Abbr_Init(&state->abbreviations);

  WSH_PAL_SetupWinEnv(state);

  return state;
}

void WSH_State_Destroy(wsh_state_t *state) {
  if (!state)
    return;

  for (u32 i = 0; i < state->env_count; i++) {
    free(state->env_vars[i].key);
    free(state->env_vars[i].value);
  }
  free(state->env_vars);

  WSH_History_Destroy(&state->history);
  WSH_Pager_Free(&state->pager);
  WSH_Abbr_Free(&state->abbreviations);
  free(state);
}

void WSH_State_SetEnv(wsh_state_t *state, const char *key, const char *value) {
  /* Check if exists */
  for (u32 i = 0; i < state->env_count; i++) {
    if (strcmp(state->env_vars[i].key, key) == 0) {
      char *new_val = strdup(value ? value : "");
      if (new_val) {
        free(state->env_vars[i].value);
        state->env_vars[i].value = new_val;
      }
      return;
    }
  }

  /* Add new */
  if (state->env_count >= state->env_capacity) {
    u32 new_capacity = state->env_capacity * 2;
    wsh_env_var_t *new_vars = (wsh_env_var_t *)realloc(
        state->env_vars, sizeof(wsh_env_var_t) * new_capacity);
    if (!new_vars)
      return;
    state->env_vars = new_vars;
    state->env_capacity = new_capacity;
  }

  char *new_key = strdup(key);
  char *new_val = strdup(value ? value : "");
  if (new_key && new_val) {
    state->env_vars[state->env_count].key = new_key;
    state->env_vars[state->env_count].value = new_val;
    state->env_count++;
  } else {
    free(new_key);
    free(new_val);
  }
}

const char *WSH_State_GetEnv(wsh_state_t *state, const char *key) {
  for (u32 i = 0; i < state->env_count; i++) {
    if (strcmp(state->env_vars[i].key, key) == 0) {
      return state->env_vars[i].value;
    }
  }
  return NULL;
}

void WSH_State_UnsetEnv(wsh_state_t *state, const char *key) {
  for (u32 i = 0; i < state->env_count; i++) {
    if (strcmp(state->env_vars[i].key, key) == 0) {
      free(state->env_vars[i].key);
      free(state->env_vars[i].value);

      /* Move last to this slot to keep packed */
      if (i < state->env_count - 1) {
        state->env_vars[i] = state->env_vars[state->env_count - 1];
      }
      state->env_count--;
      return;
    }
  }
}

void WSH_State_UpdateCWD(wsh_state_t *state) {
  if (getcwd(state->cwd, sizeof(state->cwd)) == NULL) {
    strcpy(state->cwd, "/");
  }
}
