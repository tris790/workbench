/*
 * linux_pty.c - Linux PTY implementation using forkpty()
 *
 * Spawns a shell in a pseudo-terminal with proper signal handling.
 * C99, handmade hero style.
 */

#include "../../terminal/workbench_pty.h"

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

struct PTY {
  pid_t pid;
  int master_fd;
  u32 cols;
  u32 rows;
  b32 alive;
};

/* Get default shell from environment or fallback */
static const char *get_default_shell(void) {
  const char *shell = getenv("SHELL");
  if (shell && shell[0])
    return shell;
  return "/bin/sh";
}

PTY *PTY_Create(const char *shell, const char *cwd) {
  /* Manual memory management is acceptable in the platform layer */
  PTY *pty = malloc(sizeof(PTY));
  if (!pty)
    return NULL;

  memset(pty, 0, sizeof(PTY));
  pty->cols = 80;
  pty->rows = 24;
  pty->alive = true;

  /* Set initial terminal size */
  struct winsize ws = {.ws_row = (unsigned short)pty->rows,
                       .ws_col = (unsigned short)pty->cols};

  pid_t pid = forkpty(&pty->master_fd, NULL, NULL, &ws);

  if (pid < 0) {
    /* Fork failed */
    free(pty);
    return NULL;
  }

  if (pid == 0) {
    /* Child process */

    /* Change to working directory */
    if (cwd && cwd[0]) {
      if (chdir(cwd) < 0) {
        /* Fallback to home if cwd fails */
        const char *home = getenv("HOME");
        if (home)
          if (chdir(home) < 0) {
            /* last resort */
          }
      }
    }

    /* Get shell to use */
    const char *sh = shell ? shell : get_default_shell();

    /* Extract shell name for argv[0] (convention: login shell has - prefix) */
    const char *shell_name = strrchr(sh, '/');
    shell_name = shell_name ? shell_name + 1 : sh;

    /* Execute shell */
    execlp(sh, shell_name, (char *)NULL);

    /* If exec fails, try fallback */
    execlp("/bin/sh", "sh", (char *)NULL);
    _exit(127);
  }

  /* Parent process */
  pty->pid = pid;

  /* Set non-blocking for smooth UI reads */
  int flags = fcntl(pty->master_fd, F_GETFL, 0);
  fcntl(pty->master_fd, F_SETFL, flags | O_NONBLOCK);

  return pty;
}

void PTY_Destroy(PTY *pty) {
  if (!pty)
    return;

  if (pty->alive) {
    /* Send SIGHUP to child (like closing terminal) */
    kill(pty->pid, SIGHUP);

    /* Give it a moment, then force kill if needed */
    usleep(50000); /* 50ms */

    int status;
    pid_t result = waitpid(pty->pid, &status, WNOHANG);
    if (result == 0) {
      /* Still running, force kill */
      kill(pty->pid, SIGKILL);
      waitpid(pty->pid, &status, 0);
    }
  }

  if (pty->master_fd >= 0) {
    close(pty->master_fd);
  }

  free(pty);
}

i32 PTY_Read(PTY *pty, char *buffer, u32 size) {
  if (!pty || !pty->alive || pty->master_fd < 0)
    return 0;

  ssize_t bytes = read(pty->master_fd, buffer, size);

  if (bytes > 0) {
    return (i32)bytes;
  }

  if (bytes < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      /* No data available (non-blocking) */
      return 0;
    }
    /* Real error - PTY probably closed */
    pty->alive = false;
    return -1;
  }

  /* EOF - child closed */
  pty->alive = false;
  return 0;
}

i32 PTY_Write(PTY *pty, const char *data, u32 size) {
  if (!pty || !pty->alive || pty->master_fd < 0)
    return -1;

  ssize_t written = write(pty->master_fd, data, size);
  return (i32)written;
}

void PTY_Resize(PTY *pty, u32 cols, u32 rows) {
  if (!pty || pty->master_fd < 0)
    return;

  if (cols == pty->cols && rows == pty->rows)
    return;

  pty->cols = cols;
  pty->rows = rows;

  struct winsize ws = {.ws_row = (unsigned short)rows,
                       .ws_col = (unsigned short)cols};

  ioctl(pty->master_fd, TIOCSWINSZ, &ws);
}

b32 PTY_IsAlive(PTY *pty) {
  if (!pty)
    return false;

  if (!pty->alive)
    return false;

  /* Check if child process is still running */
  int status;
  pid_t result = waitpid(pty->pid, &status, WNOHANG);

  if (result == 0) {
    /* Still running */
    return true;
  } else if (result == pty->pid) {
    /* Exited */
    pty->alive = false;
    return false;
  } else {
    /* Error (ECHILD, already reaped) */
    pty->alive = false;
    return false;
  }
}

i32 PTY_GetFD(PTY *pty) {
  if (!pty)
    return -1;
  return pty->master_fd;
}
