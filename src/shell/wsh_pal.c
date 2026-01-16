#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "wsh_pal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef __CYGWIN__
#include <sys/cygwin.h>
#include <windows.h>
#endif

/* Convert WSH env vars to char** array for execve */
static char **make_envp(wsh_state_t *state) {
  char **envp = malloc(sizeof(char *) * (state->env_count + 1));
  if (!envp)
    return NULL;

  for (u32 i = 0; i < state->env_count; i++) {
    size_t len =
        strlen(state->env_vars[i].key) + strlen(state->env_vars[i].value) + 2;
    envp[i] = malloc(len);
    snprintf(envp[i], len, "%s=%s", state->env_vars[i].key,
             state->env_vars[i].value);
  }
  envp[state->env_count] = NULL;
  return envp;
}

static i32 WSH_PAL_ExecuteInternal(wsh_state_t *state, char **argv) {
  if (!argv || !argv[0])
    return 0;

#ifdef _WIN32
  char **envp = make_envp(state);
  intptr_t ret = _spawnvpe(_P_WAIT, argv[0], (const char *const *)argv,
                           (const char *const *)envp);

  // Cleanup envp (make_envp allocates strings)
  if (envp) {
    for (int i = 0; envp[i]; i++)
      free(envp[i]);
    free(envp);
  }

  if (ret == -1)
    return 127;
  return (i32)ret;
#else
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    /* Child */
    char **envp = make_envp(state);
    execvpe(argv[0], argv, envp);

    /* If we get here, exec failed */
    /* fprintf(stderr, "wsh: command not found: %s\n", argv[0]); */
    exit(127);
  }

  /* Parent */
  int status;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 127;
#endif
}

i32 WSH_PAL_Execute(wsh_state_t *state, char **argv) {
  /* Disable raw mode so child process inherits cooked terminal */
  WSH_PAL_DisableRawMode();

  /* Try direct execution first */
  i32 ret = WSH_PAL_ExecuteInternal(state, argv);
  if (ret != 127) {
    WSH_PAL_EnableRawMode();
    return ret;
  }

  /* Try extensions if not found */
  const char *exts[] = {".exe", ".cmd", ".bat", NULL};
  for (int e = 0; exts[e]; e++) {
    char *orig = argv[0];
    size_t len = strlen(orig) + strlen(exts[e]) + 1;
    char *with_ext = malloc(len);
    sprintf(with_ext, "%s%s", orig, exts[e]);
    argv[0] = with_ext;

    if (strcmp(exts[e], ".cmd") == 0 || strcmp(exts[e], ".bat") == 0) {
      /* Need to run with cmd /c */
      int i = 0;
      while (argv[i])
        i++;
      char **new_argv = malloc(sizeof(char *) * (i + 4));
      new_argv[0] = "cmd.exe";
      new_argv[1] = "/c";
      new_argv[2] = with_ext;
      for (int j = 1; j < i; j++)
        new_argv[2 + j] = argv[j];
      new_argv[i + 2] = NULL;

      ret = WSH_PAL_ExecuteInternal(state, new_argv);
      free(new_argv);
    } else {
      ret = WSH_PAL_ExecuteInternal(state, argv);
    }

    free(with_ext);
    argv[0] = orig;

    if (ret != 127) {
      WSH_PAL_EnableRawMode();
      return ret;
    }
  }

  fprintf(stderr, "wsh: command not found: %s\n", argv[0]);
  WSH_PAL_EnableRawMode();
  return 127;
}

/* Helper to try executing with extensions - Removed helper, inlined above */

i32 WSH_PAL_Chdir(wsh_state_t *state, const char *path) {
  if (chdir(path) != 0) {
    perror("cd");
    return -1;
  }
  WSH_State_UpdateCWD(state);
  return 0;
}

b32 WSH_PAL_IsExecutable(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    if (st.st_mode & S_IXUSR)
      return true;
  }
  return false;
}

char *WSH_PAL_ExpandPath(wsh_state_t *state, const char *path) {
  if (!path)
    return NULL;

#ifdef __CYGWIN__
  if (strncmp(path, "~win", 4) == 0 && (path[4] == '\0' || path[4] == '/')) {
    char *win_home = WSH_PAL_GetWinHome();
    if (!win_home)
      return strdup(path);

    const char *rest = path + 4;
    size_t len = strlen(win_home) + strlen(rest);
    char *expanded = malloc(len + 1);
    strcpy(expanded, win_home);
    strcat(expanded, rest);
    free(win_home);
    return expanded;
  }
#endif

  if (path[0] == '~') {
    const char *home = WSH_State_GetEnv(state, "HOME");
    if (!home)
      return strdup(path);

    size_t len = strlen(home) + strlen(path); /* path includes ~ */
    char *expanded = malloc(len + 1);
    strcpy(expanded, home);
    strcat(expanded, path + 1);
    return expanded;
  }
  return strdup(path);
}

void WSH_PAL_SetupWinEnv(wsh_state_t *state) {
#ifdef __CYGWIN__
  /* Check COLORTERM */
  char *ct = getenv("COLORTERM");
  if (ct && (strcmp(ct, "truecolor") == 0 || strcmp(ct, "24bit") == 0)) {
    WSH_State_SetEnv(state, "COLORTERM", ct);
  }

  char *wh = WSH_PAL_GetWinHome();
  if (wh) {
    WSH_State_SetEnv(state, "WINHOME", wh);
    free(wh);
  }
#else
  (void)state;
#endif
}

char *WSH_PAL_GetWinHome(void) {
#ifdef __CYGWIN__
  char *profile = getenv("USERPROFILE");
  if (!profile)
    return NULL;
  ssize_t size =
      cygwin_conv_path(CCP_WIN_A_TO_POSIX | CCP_ABSOLUTE, profile, NULL, 0);
  if (size < 0)
    return NULL;
  char *posix_path = malloc(size);
  if (cygwin_conv_path(CCP_WIN_A_TO_POSIX | CCP_ABSOLUTE, profile, posix_path,
                       size) != 0) {
    free(posix_path);
    return NULL;
  }
  return posix_path;
#else
  return NULL;
#endif
}

b32 WSH_PAL_ClipboardCopy(const char *text, size_t len) {
#ifdef __CYGWIN__
  FILE *f = fopen("/dev/clipboard", "w");
  if (!f)
    return false;
  fwrite(text, 1, len, f);
  fclose(f);
  return true;
#else
  (void)text;
  (void)len;
  return false;
#endif
}

b32 WSH_PAL_ClipboardCopyFromStdin(void) {
#ifdef __CYGWIN__
  FILE *f = fopen("/dev/clipboard", "w");
  if (!f)
    return false;
  char buf[1024];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
    fwrite(buf, 1, n, f);
  }
  fclose(f);
  return true;
#else
  return false;
#endif
}

char *WSH_PAL_ClipboardPaste(void) {
#ifdef __CYGWIN__
  FILE *f = fopen("/dev/clipboard", "r");
  if (!f)
    return NULL;

  size_t capacity = 1024;
  size_t size = 0;
  char *buf = malloc(capacity);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  char chunk[1024];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
    if (size + n >= capacity) {
      capacity = (size + n) * 2;
      char *new_buf = realloc(buf, capacity);
      if (!new_buf) {
        free(buf);
        fclose(f);
        return NULL;
      }
      buf = new_buf;
    }
    memcpy(buf + size, chunk, n);
    size += n;
  }
  buf[size] = '\0';
  fclose(f);
  return buf;
#else
  return NULL;
#endif
}

const char *WSH_PAL_GetPathSeparator(void) {
#ifdef _WIN32
  return ";";
#else
  return ":";
#endif
}

i32 WSH_PAL_MkdirRecursive(const char *path) {
  char tmp[4096];
  char *p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (len == 0)
    return 0;
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;
  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
#ifdef _WIN32
      mkdir(tmp);
#else
      mkdir(tmp, S_IRWXU);
#endif
      *p = '/';
    }
  }
#ifdef _WIN32
  return mkdir(tmp);
#else
  return mkdir(tmp, S_IRWXU);
#endif
}

#ifdef _WIN32
int WSH_PAL_SelectStdin(long timeout_usec) {
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  /* WaitForSingleObject uses milliseconds */
  DWORD ms = (DWORD)(timeout_usec / 1000);
  /* Minimum 0 checks immediate, but we want small wait if 0 is passed?
     Usually select with 0 timeout is polling. 50ms is passed by caller usually.
     If timeout_usec > 0 and < 1000, WaitForSingleObject(0) is closest. */
  if (timeout_usec > 0 && ms == 0)
    ms = 1;

  DWORD result = WaitForSingleObject(hIn, ms);
  if (result == WAIT_OBJECT_0)
    return 1;
  if (result == WAIT_TIMEOUT)
    return 0;
  return -1;
}

static DWORD orig_console_mode;
#else
#include <sys/select.h>
int WSH_PAL_SelectStdin(long timeout_usec) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  struct timeval tv;
  tv.tv_sec = timeout_usec / 1000000;
  tv.tv_usec = timeout_usec % 1000000;
  return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}
#endif

#ifdef _WIN32

void WSH_PAL_DisableRawMode(void) {
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  SetConsoleMode(hIn, orig_console_mode);
}

void WSH_PAL_EnableRawMode(void) {
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(hIn, &orig_console_mode);

  static b32 atexit_registered = 0;
  if (!atexit_registered) {
    atexit(WSH_PAL_DisableRawMode);
    atexit_registered = 1;
  }

  DWORD raw = orig_console_mode;
  raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
  SetConsoleMode(hIn, raw);
}
#else
#include <termios.h>

static struct termios orig_termios;

void WSH_PAL_DisableRawMode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void WSH_PAL_EnableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    return;

  static b32 atexit_registered = 0;
  if (!atexit_registered) {
    atexit(WSH_PAL_DisableRawMode);
    atexit_registered = 1;
  }

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1; /* 100ms timeout */

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
#endif
