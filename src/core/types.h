/*
 * types.h - Core type definitions for Workbench
 *
 * Basic types, macros, and memory arena.
 * C99, handmade hero style.
 */

#ifndef TYPES_H
#define TYPES_H

#define FS_MAX_PATH 4096
#define FS_MAX_NAME 256

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

/* ===== Basic Types ===== */

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef size_t usize;
typedef ptrdiff_t isize;

typedef int32_t b32; /* Boolean (32-bit for alignment) */

/* ===== Vector Types ===== */

typedef struct {
  i32 x, y;
} v2i;
typedef struct {
  f32 x, y;
} v2f;
typedef struct {
  i32 x, y, w, h;
} rect;
/* Note: color type is defined in renderer.h */

/* ===== Utility Macros ===== */

#define ArrayCount(arr) (sizeof(arr) / sizeof((arr)[0]))

#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(val, min, max) (Min(Max(val, min), max))

#define Kilobytes(n) ((usize)(n) * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)
#define Gigabytes(n) (Megabytes(n) * 1024)

#ifdef WB_DEBUG
#include <assert.h>
#define Assert(expr) assert(expr)
#else
#define Assert(expr) ((void)0)
#endif

/* ===== Memory Arena ===== */

typedef struct {
  u8 *base;
  usize size;
  usize used;
} memory_arena;

static inline void ArenaInit(memory_arena *arena, void *base, usize size) {
  arena->base = (u8 *)base;
  arena->size = size;
  arena->used = 0;
}

static inline void *ArenaPush(memory_arena *arena, usize size) {
  /* Enforce 8-byte alignment */
  usize aligned_used = (arena->used + 7) & ~7;
  arena->used = aligned_used;

  if (arena->used + size > arena->size) {
    return NULL;
  }
  void *result = arena->base + arena->used;
  arena->used += size;
  return result;
}

#define ArenaPushArray(arena, type, count)                                     \
  ((type *)ArenaPush((arena), sizeof(type) * (count)))

#define ArenaPushStruct(arena, type) ArenaPushArray(arena, type, 1)

static inline void ArenaReset(memory_arena *arena) { arena->used = 0; }

static inline usize ArenaRemaining(memory_arena *arena) {
  return arena->size - arena->used;
}

/* ===== Temporary Memory ===== */

typedef struct {
  memory_arena *arena;
  usize used;
} temporary_memory;

static inline temporary_memory BeginTemporaryMemory(memory_arena *arena) {
  temporary_memory temp;
  temp.arena = arena;
  temp.used = arena->used;
  return temp;
}

static inline void EndTemporaryMemory(temporary_memory temp) {
  Assert(temp.arena->used >= temp.used);
  temp.arena->used = temp.used;
}

#endif /* TYPES_H */
