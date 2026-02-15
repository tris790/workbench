/*
 * task_queue.h - Background Task Queue for Workbench
 *
 * Provides asynchronous task execution with progress reporting.
 * Tasks run on a separate thread to keep the UI responsive.
 * C99, handmade hero style.
 */

#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "types.h"

/* ===== Progress Types ===== */

typedef enum {
  WB_PROGRESS_TYPE_BOUNDED,    /* Known total work (e.g., copy 100 files) */
  WB_PROGRESS_TYPE_UNBOUNDED,  /* Unknown total work (e.g., recursive delete) */
} progress_type;

typedef struct {
  progress_type type;
  union {
    struct {
      u64 current;  /* Current progress */
      u64 total;    /* Total work (can be 0 if unknown initially) */
    } bounded;
    struct {
      const char *status;  /* Status message (e.g., "Deleting /path/to/file") */
    } unbounded;
  } data;
} task_progress;

/* ===== Task Types ===== */

/* Task work function signature.
 * user_data: Task-specific data
 * progress:  Function to call to report progress (can be NULL)
 * Returns true on success, false on failure. */
typedef b32 (*task_work_fn)(void *user_data, void (*progress)(const task_progress *));

/* Task cleanup function signature (called on main thread after task completes). */
typedef void (*task_cleanup_fn)(void *user_data, b32 success);

typedef struct task_item_s {
  task_work_fn work;         /* Work function (runs on worker thread) */
  task_cleanup_fn cleanup;   /* Cleanup function (runs on main thread after complete) */
  void *user_data;           /* Task-specific data */
  struct task_item_s *next;  /* Next item in queue */
} task_item;

/* ===== Task Queue State ===== */

typedef struct {
  task_item *head;           /* Head of queue (next to execute) */
  task_item *tail;           /* Tail of queue (last added) */
  task_item *current;        /* Currently executing task (NULL if idle) */
  b32 is_running;            /* Is a task currently running? */
  b32 shutdown_requested;    /* Signal to worker thread to shut down */
  
  /* Threading */
  void *thread;              /* Platform thread handle */
  void *mutex;               /* Mutex for queue access */
  void *cond_var;            /* Condition variable for signaling */
  
  /* Progress tracking */
  task_progress current_progress;
  u64 task_start_time_ms;    /* When current task started */
  
  /* Memory arena for allocations */
  memory_arena *arena;
} task_queue;

/* ===== Task Queue API ===== */

/* Initialize the task queue system.
 * Must be called before any other task functions. */
void TaskQueue_Init(task_queue *queue, memory_arena *arena);

/* Shutdown the task queue system.
 * Waits for current task to complete (if any) and clears the queue. */
void TaskQueue_Shutdown(task_queue *queue);

/* Queue a new background task.
 * The task will be executed on a worker thread.
 * cleanup is called on the main thread after work completes. */
void TaskQueue_Submit(task_queue *queue, task_work_fn work, task_cleanup_fn cleanup, void *user_data);

/* Check if any tasks are running or queued.
 * Returns true if there's work in progress. */
b32 TaskQueue_IsBusy(task_queue *queue);

/* Get the number of queued tasks (not including currently running). */
i32 TaskQueue_GetQueueSize(task_queue *queue);

/* Get current progress of the running task.
 * Returns NULL if no task is running. */
const task_progress *TaskQueue_GetProgress(task_queue *queue);

/* Get elapsed time of current task in milliseconds.
 * Returns 0 if no task is running. */
u64 TaskQueue_GetElapsedMs(task_queue *queue);

/* Update the task queue (call from main thread each frame).
 * Processes completed tasks and runs cleanup functions. */
void TaskQueue_Update(task_queue *queue);

/* Cancel all pending tasks (does not cancel running task). */
void TaskQueue_ClearPending(task_queue *queue);

#endif /* TASK_QUEUE_H */
