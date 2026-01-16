#ifndef WSH_PAL_H
#define WSH_PAL_H

#include "wsh.h"
#include "wsh_state.h"

/* Platform Abstraction Layer */

/* Execute a process. Returns exit code. */
i32 WSH_PAL_Execute(wsh_state_t *state, char **argv);

/* Change directory */
i32 WSH_PAL_Chdir(wsh_state_t *state, const char *path);

/* Check if path is executable */
b32 WSH_PAL_IsExecutable(const char *path);

/* Expand path (tilde expansion etc) - returns new string to be freed */
char *WSH_PAL_ExpandPath(wsh_state_t *state, const char *path);

void WSH_PAL_EnableRawMode(void);
void WSH_PAL_DisableRawMode(void);

void WSH_PAL_SetupWinEnv(wsh_state_t *state);
char *WSH_PAL_GetWinHome(void);
b32 WSH_PAL_ClipboardCopy(const char *text, size_t len);
/* If len is 0, read from STDIN until EOF */
b32 WSH_PAL_ClipboardCopyFromStdin(void);
char *WSH_PAL_ClipboardPaste(void);
const char *WSH_PAL_GetPathSeparator(void);
i32 WSH_PAL_MkdirRecursive(const char *path);
/* Wait for stdin input with timeout in microseconds. Returns >0 if data ready,
 * 0 on timeout, -1 on error. */
int WSH_PAL_SelectStdin(long timeout_usec);

#endif /* WSH_PAL_H */
