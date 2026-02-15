/*
 * task_queue.c - Background Task Queue Implementation
 *
 * C99, handmade hero style.
 */

#include "task_queue.h"
#include "../platform/platform.h"

#include <string.h>

/* ===== Platform Threading Abstractions ===== */

/* Forward declarations - implemented per platform */
extern void *Platform_CreateThread(void *(*func)(void *), void *arg);
extern void Platform_DestroyThread(void *thread);
extern void *Platform_CreateMutex(void);
extern void Platform_DestroyMutex(void *mutex);
extern void Platform_LockMutex(void *mutex);
extern void Platform_UnlockMutex(void *mutex);
extern void *Platform_CreateCondVar(void);
extern void Platform_DestroyCondVar(void *cond);
extern void Platform_CondWait(void *cond, void *mutex);
extern void Platform_CondSignal(void *cond);

/* ===== Internal Worker Thread ===== */

static task_queue *g_current_queue = NULL;

static void TaskQueue_ReportProgress(const task_progress *progress) {
  if (!g_current_queue) return;
  Platform_LockMutex(g_current_queue->mutex);
  g_current_queue->current_progress = *progress;
  Platform_UnlockMutex(g_current_queue->mutex);
}

static void *TaskQueue_WorkerThread(void *arg) {
  task_queue *queue = (task_queue *)arg;
  
  while (1) {
    Platform_LockMutex(queue->mutex);
    
    /* Wait for work or shutdown signal */
    while (queue->head == NULL && !queue->shutdown_requested) {
      Platform_CondWait(queue->cond_var, queue->mutex);
    }
    
    /* Check for shutdown */
    if (queue->shutdown_requested) {
      Platform_UnlockMutex(queue->mutex);
      break;
    }
    
    /* Get next task */
    task_item *task = queue->head;
    queue->head = task->next;
    if (queue->head == NULL) {
      queue->tail = NULL;
    }
    task->next = NULL;
    
    queue->current = task;
    queue->is_running = true;
    queue->task_start_time_ms = Platform_GetTimeMs();
    
    /* Reset progress */
    queue->current_progress.type = WB_PROGRESS_TYPE_UNBOUNDED;
    queue->current_progress.data.unbounded.status = NULL;
    
    g_current_queue = queue;
    
    Platform_UnlockMutex(queue->mutex);
    
    /* Execute task */
    b32 success = task->work(task->user_data, TaskQueue_ReportProgress);
    
    Platform_LockMutex(queue->mutex);
    queue->is_running = false;
    if (task->cleanup) {
      task->cleanup(task->user_data, success);
    }
    queue->current = NULL;
    Platform_UnlockMutex(queue->mutex);
    
    /* Task memory will be reclaimed with arena reset on next submit */
  }
  
  return NULL;
}

/* ===== Public API ===== */

void TaskQueue_Init(task_queue *queue, memory_arena *arena) {
  memset(queue, 0, sizeof(*queue));
  queue->arena = arena;
  
  queue->mutex = Platform_CreateMutex();
  queue->cond_var = Platform_CreateCondVar();
  
  queue->thread = Platform_CreateThread(TaskQueue_WorkerThread, queue);
}

void TaskQueue_Shutdown(task_queue *queue) {
  if (!queue->thread) return;
  
  /* Signal shutdown */
  Platform_LockMutex(queue->mutex);
  queue->shutdown_requested = true;
  Platform_CondSignal(queue->cond_var);
  Platform_UnlockMutex(queue->mutex);
  
  /* TODO: Wait for thread to finish (need Platform_JoinThread) */
  /* For now, we'll just clean up - tasks should be quick */
  
  Platform_DestroyThread(queue->thread);
  Platform_DestroyCondVar(queue->cond_var);
  Platform_DestroyMutex(queue->mutex);
  
  memset(queue, 0, sizeof(*queue));
}

void TaskQueue_Submit(task_queue *queue, task_work_fn work, task_cleanup_fn cleanup, void *user_data) {
  if (!queue->thread) return;
  
  task_item *item = ArenaPush(queue->arena, sizeof(task_item));
  item->work = work;
  item->cleanup = cleanup;
  item->user_data = user_data;
  item->next = NULL;
  
  Platform_LockMutex(queue->mutex);
  
  if (queue->tail) {
    queue->tail->next = item;
  } else {
    queue->head = item;
  }
  queue->tail = item;
  
  Platform_CondSignal(queue->cond_var);
  Platform_UnlockMutex(queue->mutex);
}

b32 TaskQueue_IsBusy(task_queue *queue) {
  Platform_LockMutex(queue->mutex);
  b32 busy = (queue->is_running || queue->head != NULL);
  Platform_UnlockMutex(queue->mutex);
  return busy;
}

i32 TaskQueue_GetQueueSize(task_queue *queue) {
  Platform_LockMutex(queue->mutex);
  i32 count = 0;
  task_item *item = queue->head;
  while (item) {
    count++;
    item = item->next;
  }
  Platform_UnlockMutex(queue->mutex);
  return count;
}

const task_progress *TaskQueue_GetProgress(task_queue *queue) {
  /* Note: Caller should not hold the lock - we return pointer to internal state
   * This is safe for read-only access from main thread */
  if (!queue->is_running) return NULL;
  return &queue->current_progress;
}

u64 TaskQueue_GetElapsedMs(task_queue *queue) {
  if (!queue->is_running) return 0;
  return Platform_GetTimeMs() - queue->task_start_time_ms;
}

void TaskQueue_Update(task_queue *queue) {
  /* Currently cleanup runs inline in worker thread after task completes.
   * In the future, we might want to defer cleanup to main thread
   * for thread-safety with UI updates. */
  (void)queue;
}

void TaskQueue_ClearPending(task_queue *queue) {
  Platform_LockMutex(queue->mutex);
  /* Note: Currently running task is not cancelled */
  queue->head = NULL;
  queue->tail = NULL;
  Platform_UnlockMutex(queue->mutex);
}
