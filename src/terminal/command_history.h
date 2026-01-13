/*
 * command_history.h - Command history storage for autosuggestions
 *
 * Manages persistent command history for fish-style autosuggestions.
 * Uses a ring buffer with LRU replacement.
 * C99, handmade hero style.
 */

#ifndef COMMAND_HISTORY_H
#define COMMAND_HISTORY_H

#include "../core/types.h"

/* Configuration */
#define HISTORY_MAX_ENTRIES 10000
#define HISTORY_MAX_LENGTH 1024
#define HISTORY_DEFAULT_FILE ".workbench_history"

typedef struct CommandHistory CommandHistory;

struct CommandHistory {
  char (*entries)[HISTORY_MAX_LENGTH]; /* Array of command strings */
  u32 count;                           /* Number of valid entries */
  u32 capacity;                        /* Max entries (HISTORY_MAX_ENTRIES) */
  u32 head;                            /* Index of most recent entry */
  char filepath[512];                  /* Path to history file */
  b32 dirty;                           /* Needs saving */
};

/* Create history manager, optionally loading from file */
CommandHistory *History_Create(const char *filepath);

/* Destroy history and free resources */
void History_Destroy(CommandHistory *hist);

/* Add command to history (deduplicates, moves to front if exists) */
void History_Add(CommandHistory *hist, const char *command);

/* Search for command starting with prefix, returns most recent match */
const char *History_SearchPrefix(CommandHistory *hist, const char *prefix);

/* Get entry by index (0 = most recent) */
const char *History_Get(CommandHistory *hist, u32 index);

/* Get total count of entries */
u32 History_Count(CommandHistory *hist);

/* Save history to file */
void History_Save(CommandHistory *hist);

/* Load history from file */
void History_Load(CommandHistory *hist);

#endif /* COMMAND_HISTORY_H */
