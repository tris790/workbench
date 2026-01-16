#ifndef WSH_HIGHLIGHT_H
#define WSH_HIGHLIGHT_H

#include "wsh_state.h"

// Returns a malloc-ed string with ANSI escape codes.
// Caller must free.
char *WSH_Highlight(wsh_state_t *state, const char *input);

#endif
