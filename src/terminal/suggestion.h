/*
 * suggestion.h - Fish-style autosuggestion engine
 *
 * Provides command suggestions based on history and file paths.
 * C99, handmade hero style.
 */

#ifndef SUGGESTION_H
#define SUGGESTION_H

#include "../core/types.h"
#include "command_history.h"

/* Source of suggestion */
typedef enum {
  SUGGESTION_NONE = 0,
  SUGGESTION_HISTORY, /* From command history */
  SUGGESTION_PATH,    /* File/directory path completion */
  SUGGESTION_BUILTIN, /* Built-in command completion */
} suggestion_source;

/* Suggestion result */
typedef struct {
  char full_text[1024]; /* The complete suggested command */
  char suffix[1024];    /* Part after what user typed (ghost text) */
  suggestion_source source;
  b32 valid;
} Suggestion;

/* Suggestion engine state */
typedef struct SuggestionEngine SuggestionEngine;

struct SuggestionEngine {
  CommandHistory *history;
  char current_cwd[512];
  Suggestion current;
};

/* Create suggestion engine */
SuggestionEngine *Suggestion_Create(const char *history_path);

/* Destroy suggestion engine */
void Suggestion_Destroy(SuggestionEngine *engine);

/* Update current working directory (for path suggestions) */
void Suggestion_SetCWD(SuggestionEngine *engine, const char *cwd);

/* Get suggestion for partial input */
Suggestion Suggestion_Get(SuggestionEngine *engine, const char *input);

/* Record executed command to history */
void Suggestion_RecordCommand(SuggestionEngine *engine, const char *command);

/* Get the suffix text to display as ghost text */
const char *Suggestion_GetSuffix(Suggestion *sug);

/* Extract first word from suggestion suffix (for Alt+Right behavior) */
void Suggestion_GetFirstWord(Suggestion *sug, char *out_word, u32 max_len);

#endif /* SUGGESTION_H */
