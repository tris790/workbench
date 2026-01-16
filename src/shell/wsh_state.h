#ifndef WSH_STATE_H
#define WSH_STATE_H

#include "wsh.h"
#include "wsh_abbr.h"
#include "wsh_completion.h"
#include "wsh_history.h"

typedef struct {
  char *key;
  char *value;
} wsh_env_var_t;

typedef struct {
  /* Environment */
  wsh_env_var_t *env_vars;
  u32 env_count;
  u32 env_capacity;

  /* State */
  char cwd[4096];
  b32 running;
  i32 last_exit_code;

  /* History */
  wsh_history_t history;

  /* Completions */
  wsh_pager_t pager;

  /* Abbreviations */
  wsh_abbr_map_t abbreviations;
} wsh_state_t;

/* Initialize new shell state */
wsh_state_t *WSH_State_Create(int argc, char **argv, char **envp);

/* Destroy state */
void WSH_State_Destroy(wsh_state_t *state);

/* Environment manipulation */
void WSH_State_SetEnv(wsh_state_t *state, const char *key, const char *value);
const char *WSH_State_GetEnv(wsh_state_t *state, const char *key);
void WSH_State_UnsetEnv(wsh_state_t *state, const char *key);

/* Update CWD in state */
void WSH_State_UpdateCWD(wsh_state_t *state);

#endif /* WSH_STATE_H */
