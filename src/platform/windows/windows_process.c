#ifdef _WIN32
/*
 * windows_process.c - Windows process spawning and management
 *
 * Uses CreateProcess with redirected pipes for terminal integration.
 * C99, handmade hero style.
 */

#include "windows_internal.h"

struct platform_process {
  HANDLE process_handle;
  HANDLE thread_handle;
  HANDLE stdin_write; /* Write to child's stdin */
  HANDLE stdout_read; /* Read from child's stdout */
  b32 running;
  i32 exit_code;
};

platform_process *Platform_SpawnProcess(const char *command,
                                        const char *working_dir) {
  HANDLE stdin_read = NULL, stdin_write = NULL;
  HANDLE stdout_read = NULL, stdout_write = NULL;

  /* Create pipes with inheritable handles */
  SECURITY_ATTRIBUTES sa = {0};
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
    return NULL;

  /* Ensure read handle is not inherited */
  if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(stdout_read);
    CloseHandle(stdout_write);
    return NULL;
  }

  if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
    CloseHandle(stdout_read);
    CloseHandle(stdout_write);
    return NULL;
  }

  /* Ensure write handle is not inherited */
  if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(stdout_read);
    CloseHandle(stdout_write);
    CloseHandle(stdin_read);
    CloseHandle(stdin_write);
    return NULL;
  }

  /* Setup startup info */
  STARTUPINFOW si = {0};
  si.cb = sizeof(STARTUPINFOW);
  si.hStdError = stdout_write;
  si.hStdOutput = stdout_write;
  si.hStdInput = stdin_read;
  si.dwFlags = STARTF_USESTDHANDLES;

  PROCESS_INFORMATION pi = {0};

  /* Convert command to wide string */
  /* Wrap in cmd.exe /c to handle shell commands */
  char cmdline[FS_MAX_PATH];
  snprintf(cmdline, sizeof(cmdline), "cmd.exe /c %s", command);

  wchar_t wide_cmdline[FS_MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, cmdline, -1, wide_cmdline, FS_MAX_PATH);

  wchar_t wide_working_dir[FS_MAX_PATH];
  wchar_t *working_dir_ptr = NULL;
  if (working_dir) {
    MultiByteToWideChar(CP_UTF8, 0, working_dir, -1, wide_working_dir,
                        FS_MAX_PATH);
    working_dir_ptr = wide_working_dir;
  }

  BOOL success = CreateProcessW(NULL, /* Application name (use command line) */
                                wide_cmdline, /* Command line (modifiable) */
                                NULL,         /* Process security attributes */
                                NULL,         /* Thread security attributes */
                                TRUE,         /* Inherit handles */
                                CREATE_NO_WINDOW, /* Creation flags */
                                NULL,             /* Environment */
                                working_dir_ptr,  /* Working directory */
                                &si,              /* Startup info */
                                &pi               /* Process info */
  );

  /* Close pipe ends that child uses */
  CloseHandle(stdout_write);
  CloseHandle(stdin_read);

  if (!success) {
    CloseHandle(stdout_read);
    CloseHandle(stdin_write);
    return NULL;
  }

  /* NOTE: Using malloc instead of arena allocation because process handles are
   * long-lived and explicitly freed via Platform_ProcessFree(). Arena would not
   * be appropriate since process lifetime is independent of frame/request
   * scope. */
  platform_process *process = malloc(sizeof(platform_process));
  process->process_handle = pi.hProcess;
  process->thread_handle = pi.hThread;
  process->stdin_write = stdin_write;
  process->stdout_read = stdout_read;
  process->running = true;
  process->exit_code = 0;

  return process;
}

b32 Platform_ProcessIsRunning(platform_process *process) {
  if (!process)
    return false;
  if (!process->running)
    return false;

  DWORD exit_code;
  if (GetExitCodeProcess(process->process_handle, &exit_code)) {
    if (exit_code != STILL_ACTIVE) {
      process->running = false;
      process->exit_code = (i32)exit_code;
      return false;
    }
  }

  return true;
}

i32 Platform_ProcessRead(platform_process *process, char *buffer,
                         usize buffer_size) {
  if (!process || buffer_size == 0)
    return 0;

  /* Check if data is available (non-blocking) */
  DWORD bytes_available = 0;
  if (!PeekNamedPipe(process->stdout_read, NULL, 0, NULL, &bytes_available,
                     NULL)) {
    return 0;
  }

  if (bytes_available == 0)
    return 0;

  DWORD to_read =
      (bytes_available < buffer_size) ? bytes_available : (DWORD)buffer_size;
  DWORD bytes_read = 0;

  if (!ReadFile(process->stdout_read, buffer, to_read, &bytes_read, NULL)) {
    return 0;
  }

  return (i32)bytes_read;
}

i32 Platform_ProcessWrite(platform_process *process, const char *data,
                          usize size) {
  if (!process || !process->running)
    return -1;

  DWORD bytes_written = 0;
  if (!WriteFile(process->stdin_write, data, (DWORD)size, &bytes_written,
                 NULL)) {
    return -1;
  }

  return (i32)bytes_written;
}

void Platform_ProcessKill(platform_process *process) {
  if (process && process->running) {
    TerminateProcess(process->process_handle, 1);
  }
}

void Platform_ProcessFree(platform_process *process) {
  if (!process)
    return;

  if (process->running) {
    Platform_ProcessKill(process);
  }

  CloseHandle(process->stdin_write);
  CloseHandle(process->stdout_read);
  CloseHandle(process->thread_handle);
  CloseHandle(process->process_handle);

  free(process);
}
#endif /* _WIN32 */
