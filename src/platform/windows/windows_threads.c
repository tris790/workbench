/*
 * windows_threads.c - Windows threading implementation for Workbench
 *
 * C99, handmade hero style.
 */

#include "windows_internal.h"
#include <process.h>

/* ===== Thread Implementation ===== */

typedef struct {
  void *(*func)(void *);
  void *arg;
} thread_wrapper_args;

static unsigned __stdcall ThreadWrapper(void *arg) {
  thread_wrapper_args *wrapper = (thread_wrapper_args *)arg;
  void *(*func)(void *) = wrapper->func;
  void *func_arg = wrapper->arg;
  free(wrapper);
  func(func_arg);
  return 0;
}

void *Platform_CreateThread(void *(*func)(void *), void *arg) {
  thread_wrapper_args *wrapper = (thread_wrapper_args *)malloc(sizeof(thread_wrapper_args));
  if (!wrapper) return NULL;
  
  wrapper->func = func;
  wrapper->arg = arg;
  
  uintptr_t handle = _beginthreadex(NULL, 0, ThreadWrapper, wrapper, 0, NULL);
  if (handle == 0) {
    free(wrapper);
    return NULL;
  }
  
  return (void *)handle;
}

void Platform_DestroyThread(void *thread) {
  if (!thread) return;
  HANDLE h = (HANDLE)thread;
  CloseHandle(h);
}

/* ===== Mutex Implementation =====
 * 
 * We use SRWLOCK (Slim Reader-Writer Lock) instead of Mutex because:
 * 1. SRWLOCK is much lighter weight than Mutex objects
 * 2. SRWLOCK can be used with CONDITION_VARIABLE
 * 3. We only need exclusive locking (writer mode), so SRWLOCK acts like a mutex
 */

typedef struct {
  SRWLOCK lock;
} windows_mutex;

void *Platform_CreateMutex(void) {
  windows_mutex *m = (windows_mutex *)malloc(sizeof(windows_mutex));
  if (!m) return NULL;
  
  InitializeSRWLock(&m->lock);
  
  return m;
}

void Platform_DestroyMutex(void *mutex) {
  if (!mutex) return;
  /* SRWLOCK doesn't need explicit destruction */
  free(mutex);
}

void Platform_LockMutex(void *mutex) {
  if (!mutex) return;
  windows_mutex *m = (windows_mutex *)mutex;
  AcquireSRWLockExclusive(&m->lock);
}

void Platform_UnlockMutex(void *mutex) {
  if (!mutex) return;
  windows_mutex *m = (windows_mutex *)mutex;
  ReleaseSRWLockExclusive(&m->lock);
}

/* ===== Condition Variable Implementation =====
 *
 * Windows CONDITION_VARIABLE works with SRWLOCK.
 * The key fix: we must use the passed mutex (which is our SRWLOCK wrapper)
 * with SleepConditionVariableSRW, not an internal lock.
 */

typedef struct {
  CONDITION_VARIABLE cond;
} windows_cond_var;

void *Platform_CreateCondVar(void) {
  windows_cond_var *cv = (windows_cond_var *)malloc(sizeof(windows_cond_var));
  if (!cv) return NULL;
  
  InitializeConditionVariable(&cv->cond);
  
  return cv;
}

void Platform_DestroyCondVar(void *cond) {
  if (!cond) return;
  /* CONDITION_VARIABLE doesn't need explicit destruction */
  free(cond);
}

void Platform_CondWait(void *cond, void *mutex) {
  if (!cond || !mutex) return;
  windows_cond_var *cv = (windows_cond_var *)cond;
  windows_mutex *m = (windows_mutex *)mutex;
  
  /* 
   * This atomically releases the SRWLOCK and waits on the condition variable.
   * When signaled or spuriously woken, it re-acquires the lock before returning.
   * 
   * Note: We use 0 as flags (not CONDITION_VARIABLE_INIT), which means:
   * - The lock is released while waiting
   * - The lock is re-acquired on wake
   */
  SleepConditionVariableSRW(&cv->cond, &m->lock, INFINITE, 0);
}

void Platform_CondSignal(void *cond) {
  if (!cond) return;
  windows_cond_var *cv = (windows_cond_var *)cond;
  WakeConditionVariable(&cv->cond);
}
