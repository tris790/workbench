#ifndef WSH_COMPLETION_H
#define WSH_COMPLETION_H

#include <stdbool.h>

typedef struct {
  char *display;     /* Text shown in the list */
  char *value;       /* Actual value to insert */
  char *description; /* Optional description */
} wsh_completion_t;

typedef struct {
  wsh_completion_t *candidates;
  int count;
  int capacity;
  int selected_index;
  bool active;
  char *filter; /* The word being completed */
  int filter_len;
} wsh_pager_t;

/* Initialize pager */
void WSH_Pager_Init(wsh_pager_t *pager);

/* Free pager resources */
void WSH_Pager_Free(wsh_pager_t *pager);

/* Clear current candidates */
void WSH_Pager_Clear(wsh_pager_t *pager);

/* Add a candidate */
void WSH_Pager_Add(wsh_pager_t *pager, const char *display, const char *value,
                   const char *desc);

/* Generate completions based on current input and cursor position */
void WSH_Complete(wsh_pager_t *pager, const char *line, int cursor_pos,
                  const char *cwd);

#endif /* WSH_COMPLETION_H */
