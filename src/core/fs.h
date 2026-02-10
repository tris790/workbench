/*
 * fs.h - File System Model for Workbench
 *
 * Directory state, file entries, and file type detection.
 * C99, handmade hero style.
 */

#ifndef FS_H
#define FS_H

#include "types.h"

/* ===== Configuration ===== */

#define FS_MAX_ENTRIES 2048

/* ===== File Icon Types ===== */

typedef enum {
  WB_FILE_ICON_UNKNOWN = 0,
  WB_FILE_ICON_DIRECTORY,
  WB_FILE_ICON_FILE,
  WB_FILE_ICON_CODE_C,
  WB_FILE_ICON_CODE_H,
  WB_FILE_ICON_CODE_PY,
  WB_FILE_ICON_CODE_JS,
  WB_FILE_ICON_CODE_OTHER,
  WB_FILE_ICON_IMAGE,
  WB_FILE_ICON_DOCUMENT,
  WB_FILE_ICON_ARCHIVE,
  WB_FILE_ICON_EXECUTABLE,
  WB_FILE_ICON_AUDIO,
  WB_FILE_ICON_VIDEO,
  WB_FILE_ICON_CONFIG,
  WB_FILE_ICON_MARKDOWN,
  WB_FILE_ICON_SYMLINK,
  WB_FILE_ICON_COUNT
} file_icon_type;

/* ===== Sort Options ===== */

typedef enum { WB_SORT_BY_NAME = 0, WB_SORT_BY_SIZE, WB_SORT_BY_DATE } sort_type;

typedef enum { WB_SORT_ASCENDING = 0, WB_SORT_DESCENDING } sort_order;

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
  i32 selected_index; /* Primary selection (for single-click nav) */

  sort_type sort_by;   /* Current sort field */
  sort_order sort_dir; /* Current sort direction */

  /* === NEW: Multi-selection support === */
  u8 selected[FS_MAX_ENTRIES / 8]; /* Bitmask: 1 bit per entry */
  i32 selection_count;             /* Number of selected items */
  i32 selection_anchor;            /* Anchor for shift-click range select */

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

/* Set sort options and re-sort the directory */
void FS_SetSortOptions(fs_state *state, sort_type type, sort_order order);

/* Re-sort the current entries using current state sort options */
void FS_Resort(fs_state *state);

/* Get currently selected entry (NULL if none) */
fs_entry *FS_GetSelectedEntry(fs_state *state);

/* Get entry at index (NULL if out of bounds) */
fs_entry *FS_GetEntry(fs_state *state, i32 index);

/* Set selection index (clamped to valid range) */
void FS_SetSelection(fs_state *state, i32 index);

/* Move selection up/down */
void FS_MoveSelection(fs_state *state, i32 delta);

/* ===== Multi-Selection API ===== */

/* Clear all selections and select single item at index */
void FS_SelectSingle(fs_state *state, i32 index);

/* Toggle selection at index without affecting others (Ctrl+Click) */
void FS_SelectToggle(fs_state *state, i32 index);

/* Select range from 'from' to 'to' inclusive (Shift+Click) */
void FS_SelectRange(fs_state *state, i32 from, i32 to);

/* Select all entries */
void FS_SelectAll(fs_state *state);

/* Clear all selections */
void FS_ClearSelection(fs_state *state);

/* Check if entry at index is selected */
b32 FS_IsSelected(fs_state *state, i32 index);

/* Get count of selected items */
i32 FS_GetSelectionCount(fs_state *state);

/* Get index of first selected item (-1 if none) */
i32 FS_GetFirstSelected(fs_state *state);

/* Get next selected index after 'after' (-1 if no more) - for iteration */
i32 FS_GetNextSelected(fs_state *state, i32 after);

/* Get icon type for file based on extension */
file_icon_type FS_GetIconType(const char *filename, b32 is_directory);

/* Get current working directory */
const char *FS_GetCurrentPath(fs_state *state);

/* Get home directory path */
const char *FS_GetHomePath(void);

/* Navigate to home directory */
b32 FS_NavigateHome(fs_state *state);

/* ===== File Operations ===== */

/* Delete file or recursively delete directory */
b32 FS_Delete(const char *path, memory_arena *arena);

/* Rename file or directory */
b32 FS_Rename(const char *old_path, const char *new_path);

/* Create new directory */
b32 FS_CreateDirectory(const char *path);

/* Create new empty file */
b32 FS_CreateFile(const char *path);

/* Copy file to destination */
b32 FS_Copy(const char *src, const char *dst);

/* Copy file or recursively copy directory to destination.
 * Creates destination directory if needed.
 * Returns true on success. */
b32 FS_CopyRecursive(const char *src, const char *dst, memory_arena *arena);

/* Check if file or directory exists */
b32 FS_Exists(const char *path);

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

/* Path utilities */
b32 FS_IsPathSeparator(char c);
b32 FS_PathsEqual(const char *p1, const char *p2);
const char *FS_FindLastSeparator(const char *path);
void FS_NormalizePath(char *path);
b32 FS_IsWindowsDriveRoot(const char *path);

/* Resolve path (handles ~ and relative paths) */
b32 FS_ResolvePath(const char *path, char *out_path, usize out_size);

/* Find the deepest valid parent directory for a path.
 * Walks up the path components and returns the longest prefix that exists.
 * Useful when user types a path that doesn't fully exist yet.
 * Returns true if a valid directory was found, false otherwise. */
b32 FS_FindDeepestValidDirectory(const char *path, char *out_dir,
                                  usize out_size);

#endif /* FS_H */
