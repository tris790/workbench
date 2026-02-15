/*
 * linux_threads.c - Linux threading implementation for Workbench
 *
 * C99, handmade hero style.
 */

#include "linux_internal.h"
#include <pthread.h>

/* ===== Thread Implementation ===== */

typedef struct {
  void *(*func)(void *);
  void *arg;
} thread_wrapper_args;

static void *ThreadWrapper(void *arg) {
  thread_wrapper_args *wrapper = (thread_wrapper_args *)arg;
  void *(*func)(void *) = wrapper->func;
  void *func_arg = wrapper->arg;
  free(wrapper);
  return func(func_arg);
}

void *Platform_CreateThread(void *(*func)(void *), void *arg) {
  pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
  if (!thread) return NULL;
  
  thread_wrapper_args *wrapper = (thread_wrapper_args *)malloc(sizeof(thread_wrapper_args));
  if (!wrapper) {
    free(thread);
    return NULL;
  }
  
  wrapper->func = func;
  wrapper->arg = arg;
  
  if (pthread_create(thread, NULL, ThreadWrapper, wrapper) != 0) {
    free(wrapper);
    free(thread);
    return NULL;
  }
  
  return thread;
}

void Platform_DestroyThread(void *thread) {
  if (!thread) return;
  pthread_t *t = (pthread_t *)thread;
  pthread_detach(*t);  /* Detach so resources are freed when thread exits */
  free(t);
}

/* ===== Mutex Implementation ===== */

void *Platform_CreateMutex(void) {
  pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  if (!mutex) return NULL;
  
  if (pthread_mutex_init(mutex, NULL) != 0) {
    free(mutex);
    return NULL;
  }
  
  return mutex;
}

void Platform_DestroyMutex(void *mutex) {
  if (!mutex) return;
  pthread_mutex_t *m = (pthread_mutex_t *)mutex;
  pthread_mutex_destroy(m);
  free(m);
}

void Platform_LockMutex(void *mutex) {
  if (!mutex) return;
  pthread_mutex_lock((pthread_mutex_t *)mutex);
}

void Platform_UnlockMutex(void *mutex) {
  if (!mutex) return;
  pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

/* ===== Condition Variable Implementation ===== */

void *Platform_CreateCondVar(void) {
  pthread_cond_t *cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
  if (!cond) return NULL;
  
  if (pthread_cond_init(cond, NULL) != 0) {
    free(cond);
    return NULL;
  }
  
  return cond;
}

void Platform_DestroyCondVar(void *cond) {
  if (!cond) return;
  pthread_cond_t *c = (pthread_cond_t *)cond;
  pthread_cond_destroy(c);
  free(c);
}

void Platform_CondWait(void *cond, void *mutex) {
  if (!cond || !mutex) return;
  pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}

void Platform_CondSignal(void *cond) {
  if (!cond) return;
  pthread_cond_signal((pthread_cond_t *)cond);
}
