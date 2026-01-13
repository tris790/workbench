/*
 * workbench_pty.h - Platform-agnostic PTY interface for Workbench
 *
 * Provides pseudo-terminal functionality for the built-in terminal.
 * C99, handmade hero style.
 */

#ifndef WORKBENCH_PTY_H
#define WORKBENCH_PTY_H

#include "../core/types.h"

typedef struct PTY PTY;

/* Create a new PTY with the given shell and working directory.
 * shell: Path to shell (e.g., "/usr/bin/fish") or NULL for default
 * cwd:   Working directory for the shell
 * Returns: PTY handle or NULL on failure
 */
PTY *PTY_Create(const char *shell, const char *cwd);

/* Destroy PTY and kill child process */
void PTY_Destroy(PTY *pty);

/* Read from PTY (non-blocking, returns bytes read or 0 if no data) */
i32 PTY_Read(PTY *pty, char *buffer, u32 size);

/* Write to PTY (keyboard input from user) */
i32 PTY_Write(PTY *pty, const char *data, u32 size);

/* Resize PTY to new dimensions */
void PTY_Resize(PTY *pty, u32 cols, u32 rows);

/* Check if child process is still alive */
b32 PTY_IsAlive(PTY *pty);

/* Get file descriptor for poll/select (Linux-specific, returns -1 on Windows)
 */
i32 PTY_GetFD(PTY *pty);

#endif /* WORKBENCH_PTY_H */
