/*
 * linux_process.c - Linux process spawning and management
 */

#include "linux_internal.h"
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

struct platform_process {
  pid_t pid;
  int stdin_fd;  /* Write to child's stdin */
  int stdout_fd; /* Read from child's stdout */
  int stderr_fd; /* Read from child's stderr */
  b32 running;
  i32 exit_code;
};

platform_process *Platform_SpawnProcess(const char *command,
                                        const char *working_dir) {
  int in_pipe[2];  /* Parent writes, child reads */
  int out_pipe[2]; /* Child writes, parent reads */
  int err_pipe[2]; /* Child writes, parent reads */

  if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
    return NULL;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(in_pipe[0]); close(in_pipe[1]);
    close(out_pipe[0]); close(out_pipe[1]);
    close(err_pipe[0]); close(err_pipe[1]);
    return NULL;
  }

  if (pid == 0) {
    /* Child process */
    
    /* Redirect stdin */
    if (dup2(in_pipe[0], STDIN_FILENO) < 0) exit(1);
    close(in_pipe[0]);
    close(in_pipe[1]);

    /* Redirect stdout */
    if (dup2(out_pipe[1], STDOUT_FILENO) < 0) exit(1);
    close(out_pipe[0]);
    close(out_pipe[1]);

    /* Redirect stderr */
    if (dup2(err_pipe[1], STDERR_FILENO) < 0) exit(1);
    close(err_pipe[0]);
    close(err_pipe[1]);

    if (working_dir) {
      if (chdir(working_dir) < 0) {
        exit(1);
      }
    }

    /* Execute command using shell to parse arguments */
    char *argv[] = {"/bin/sh", "-c", (char *)command, NULL};
    execvp(argv[0], argv);
    
    /* execvp only returns on error */
    exit(127);
  }

  /* Parent process */
  platform_process *process = malloc(sizeof(platform_process));
  process->pid = pid;
  process->running = true;
  process->exit_code = 0;

  /* Close unused ends */
  close(in_pipe[0]);
  close(out_pipe[1]);
  close(err_pipe[1]);

  process->stdin_fd = in_pipe[1];
  process->stdout_fd = out_pipe[0];
  process->stderr_fd = err_pipe[0];

  /* Set non-blocking read for stdout/stderr to avoid freezing UI */
  int flags = fcntl(process->stdout_fd, F_GETFL, 0);
  fcntl(process->stdout_fd, F_SETFL, flags | O_NONBLOCK);
  
  flags = fcntl(process->stderr_fd, F_GETFL, 0);
  fcntl(process->stderr_fd, F_SETFL, flags | O_NONBLOCK);

  return process;
}

b32 Platform_ProcessIsRunning(platform_process *process) {
  if (!process) return false;
  if (!process->running) return false;

  int status;
  pid_t result = waitpid(process->pid, &status, WNOHANG);
  
  if (result == 0) {
    /* Still running */
    return true;
  } else if (result == process->pid) {
    /* Exited */
    process->running = false;
    if (WIFEXITED(status)) {
      process->exit_code = WEXITSTATUS(status);
    } else {
      process->exit_code = -1;
    }
    return false;
  } else {
    /* Error (e.g. ECHILD if already waited) */
    process->running = false;
    return false;
  }
}

i32 Platform_ProcessRead(platform_process *process, char *buffer,
                         usize buffer_size) {
  if (!process || buffer_size == 0) return 0;

  /* Try reading stdout first */
  ssize_t bytes = read(process->stdout_fd, buffer, buffer_size);
  if (bytes > 0) return bytes;
  
  /* If EAGAIN, no data */
  if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return 0;
  }
  
  /* If EOF or error */
  return 0;
}

i32 Platform_ProcessWrite(platform_process *process, const char *data,
                          usize size) {
  if (!process || !process->running) return -1;
  return write(process->stdin_fd, data, size);
}

void Platform_ProcessKill(platform_process *process) {
  if (process && process->running) {
    kill(process->pid, SIGTERM);
    /* Wait slightly? or just let IsRunning clean it up later */
  }
}

void Platform_ProcessFree(platform_process *process) {
  if (!process) return;
  
  if (process->running) {
    Platform_ProcessKill(process);
  }

  close(process->stdin_fd);
  close(process->stdout_fd);
  close(process->stderr_fd);
  
  free(process);
}
