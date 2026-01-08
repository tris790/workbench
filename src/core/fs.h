/*
 * fs.h - File System Model for Workbench
 *
 * Directory state, file entries, and file type detection.
 * C99, handmade hero style.
 */

#ifndef FS_H
#define FS_H

#include "platform.h"
#include "types.h"

/* ===== Configuration ===== */

#define FS_MAX_PATH 512
#define FS_MAX_NAME 256
#define FS_MAX_ENTRIES 2048

/* ===== File Icon Types ===== */

typedef enum {
  FILE_ICON_UNKNOWN = 0,
  FILE_ICON_DIRECTORY,
  FILE_ICON_FILE,
  FILE_ICON_CODE_C,
  FILE_ICON_CODE_H,
  FILE_ICON_CODE_PY,
  FILE_ICON_CODE_JS,
  FILE_ICON_CODE_OTHER,
  FILE_ICON_IMAGE,
  FILE_ICON_DOCUMENT,
  FILE_ICON_ARCHIVE,
  FILE_ICON_EXECUTABLE,
  FILE_ICON_AUDIO,
  FILE_ICON_VIDEO,
  FILE_ICON_CONFIG,
  FILE_ICON_MARKDOWN,
  FILE_ICON_SYMLINK,
  FILE_ICON_COUNT
} file_icon_type;

/* ===== File Entry ===== */

typedef struct {
  char name[FS_MAX_NAME];
  char path[FS_MAX_PATH];
  b32 is_directory;
  u64 size;
  u64 modified_time;
  file_icon_type icon;
} fs_entry;

/* ===== Directory State ===== */

typedef struct {
  char current_path[FS_MAX_PATH];
  fs_entry *entries;
  u32 entry_count;
  u32 entry_capacity;
  i32 selected_index;

  /* For arena allocation */
  memory_arena *arena;
} fs_state;

/* ===== File System API ===== */

/* Initialize file system state */
void FS_Init(fs_state *state, memory_arena *arena);

/* Load directory contents into state */
b32 FS_LoadDirectory(fs_state *state, const char *path);

/* Navigate to parent directory */
b32 FS_NavigateUp(fs_state *state);

/* Navigate into selected directory */
b32 FS_NavigateInto(fs_state *state);

/* Get currently selected entry (NULL if none) */
fs_entry *FS_GetSelectedEntry(fs_state *state);

/* Get entry at index (NULL if out of bounds) */
fs_entry *FS_GetEntry(fs_state *state, i32 index);

/* Set selection index (clamped to valid range) */
void FS_SetSelection(fs_state *state, i32 index);

/* Move selection up/down */
void FS_MoveSelection(fs_state *state, i32 delta);

/* Get icon type for file based on extension */
file_icon_type FS_GetIconType(const char *filename, b32 is_directory);

/* Get current working directory */
const char *FS_GetCurrentPath(fs_state *state);

/* Get home directory path */
const char *FS_GetHomePath(void);

/* Navigate to home directory */
b32 FS_NavigateHome(fs_state *state);

/* ===== File Operations ===== */

/* Delete file or empty directory */
b32 FS_Delete(const char *path);

/* Rename file or directory */
b32 FS_Rename(const char *old_path, const char *new_path);

/* Create new directory */
b32 FS_CreateDirectory(const char *path);

/* Create new empty file */
b32 FS_CreateFile(const char *path);

/* Copy file to destination */
b32 FS_Copy(const char *src, const char *dst);

/* ===== Utility ===== */

/* Format file size to human readable string */
void FS_FormatSize(u64 size, char *buffer, usize buffer_size);

/* Format timestamp to human readable string */
void FS_FormatTime(u64 timestamp, char *buffer, usize buffer_size);

/* Get file extension (including dot, or empty string) */
const char *FS_GetExtension(const char *filename);

/* Get filename from full path */
const char *FS_GetFilename(const char *path);

/* Join path components */
void FS_JoinPath(char *dest, usize dest_size, const char *dir,
                 const char *filename);

#endif /* FS_H */
