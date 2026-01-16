#ifndef WSH_HISTORY_H
#define WSH_HISTORY_H

#include "wsh.h"

typedef struct {
  char *cmd;
  u64 timestamp;
  /* paths can be added here later */
} wsh_history_entry_t;

typedef struct {
  wsh_history_entry_t *entries;
  u32 count;
  u32 capacity;
} wsh_history_t;

void WSH_History_Init(wsh_history_t *hist);
void WSH_History_Destroy(wsh_history_t *hist);
void WSH_History_Load(wsh_history_t *hist);
void WSH_History_Add(wsh_history_t *hist, const char *cmd);
/* Returns the best match for autosuggestion (most recent prefix match) */
const char *WSH_History_GetSuggestion(wsh_history_t *hist, const char *prefix);

#endif /* WSH_HISTORY_H */
