/*
 * fs.c - File System Model Implementation
 *
 * C99, handmade hero style.
 */

#define _POSIX_C_SOURCE 200809L

#include "fs.h"
#include "../platform/platform.h"
#include <strings.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ===== Utility Functions ===== */

const char *FS_GetExtension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return "";
  return dot;
}

b32 FS_IsPathSeparator(char c) { return c == '/' || c == '\\'; }

const char *FS_FindLastSeparator(const char *path) {
  const char *last_slash = strrchr(path, '/');
  const char *last_backslash = strrchr(path, '\\');
  if (!last_slash)
    return last_backslash;
  if (!last_backslash)
    return last_slash;
  return (last_slash > last_backslash) ? last_slash : last_backslash;
}

b32 FS_IsWindowsDriveRoot(const char *path) {
#ifdef _WIN32
  if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
    if (path[1] == ':') {
      return true;
    }
  }
#endif
  (void)path;
  return false;
}

/* ===== Background Delete Task ===== */

b32 FS_DeleteTask_Work(void *user_data, void (*progress)(const task_progress *)) {
  fs_delete_task_data *data = (fs_delete_task_data *)user_data;
  b32 all_success = true;
  
  for (i32 i = 0; i < data->count; i++) {
    /* Report progress before each deletion */
    if (progress) {
      task_progress p = {
        .type = WB_PROGRESS_TYPE_UNBOUNDED,
        .data.unbounded.status = data->paths[i]
      };
      progress(&p);
    }
    
    /* Delete the path */
    if (!Platform_Delete(data->paths[i])) {
      all_success = false;
      /* Continue with other paths even if one fails */
    }
  }
  
  return all_success;
}

i32 FS_DeleteTask_FromSelection(fs_state *fs, fs_delete_task_data *task_data) {
  task_data->count = 0;
  
  for (i32 idx = FS_GetFirstSelected(fs); idx >= 0 && task_data->count < FS_MAX_DELETE_PATHS;
       idx = FS_GetNextSelected(fs, idx)) {
    fs_entry *entry = FS_GetEntry(fs, idx);
    if (entry && strcmp(entry->name, "..") != 0) {
      strncpy(task_data->paths[task_data->count], entry->path, FS_MAX_PATH - 1);
      task_data->paths[task_data->count][FS_MAX_PATH - 1] = '\0';
      task_data->count++;
    }
  }
  
  return task_data->count;
}

void FS_NormalizePath(char *path) {
  if (!path)
    return;
  for (char *p = path; *p; p++) {
    if (*p == '\\')
      *p = '/';
  }
  /* Remove trailing slash unless it's the root */
  usize len = strlen(path);
  if (len > 1 && path[len - 1] == '/') {
    path[len - 1] = '\0';
  }
}

b32 FS_PathsEqual(const char *p1, const char *p2) {
  if (!p1 || !p2)
    return p1 == p2;

  /* Normalize temporary copies for comparison */
  char n1[FS_MAX_PATH];
  char n2[FS_MAX_PATH];
  strncpy(n1, p1, FS_MAX_PATH - 1);
  n1[FS_MAX_PATH - 1] = '\0';
  strncpy(n2, p2, FS_MAX_PATH - 1);
  n2[FS_MAX_PATH - 1] = '\0';

  FS_NormalizePath(n1);
  FS_NormalizePath(n2);

#ifdef _WIN32
  return strcasecmp(n1, n2) == 0;
#else
  return strcmp(n1, n2) == 0;
#endif
}

const char *FS_GetFilename(const char *path) {
  const char *sep = FS_FindLastSeparator(path);
  return sep ? sep + 1 : path;
}

void FS_JoinPath(char *dest, usize dest_size, const char *dir,
                 const char *filename) {
  usize dir_len = strlen(dir);
  if (dir_len > 0 && FS_IsPathSeparator(dir[dir_len - 1])) {
    snprintf(dest, dest_size, "%s%s", dir, filename);
  } else {
    /* Use / as internal separator, FS_NormalizePath can fix it later if needed
       but Windows handles / fine. */
    snprintf(dest, dest_size, "%s/%s", dir, filename);
  }
}

void FS_FormatSize(u64 size, char *buffer, usize buffer_size) {
  if (size < 1024) {
    snprintf(buffer, buffer_size, "%llu B", (unsigned long long)size);
  } else if (size < 1024 * 1024) {
    snprintf(buffer, buffer_size, "%.1f KB", (f64)size / 1024.0);
  } else if (size < 1024 * 1024 * 1024) {
    snprintf(buffer, buffer_size, "%.1f MB", (f64)size / (1024.0 * 1024.0));
  } else {
    snprintf(buffer, buffer_size, "%.1f GB",
             (f64)size / (1024.0 * 1024.0 * 1024.0));
  }
}

void FS_FormatTime(u64 timestamp, char *buffer, usize buffer_size) {
  time_t t = (time_t)timestamp;
  struct tm *tm = localtime(&t);
  if (tm) {
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M", tm);
  } else {
    snprintf(buffer, buffer_size, "Unknown");
  }
}

/* ===== Icon Type Detection ===== */

file_icon_type FS_GetIconType(const char *filename, b32 is_directory) {
  if (is_directory) {
    return WB_FILE_ICON_DIRECTORY;
  }

  const char *ext = FS_GetExtension(filename);
  if (!ext || ext[0] == '\0') {
    /* Check if executable */
    return WB_FILE_ICON_FILE;
  }

  /* Convert to lowercase for comparison */
  char lower[16] = {0};
  for (i32 i = 0; ext[i] && i < 15; i++) {
    char c = ext[i];
    if (c >= 'A' && c <= 'Z')
      c = c + 32;
    lower[i] = c;
  }

  /* C/C++ source files */
  if (strcmp(lower, ".c") == 0 || strcmp(lower, ".cpp") == 0 ||
      strcmp(lower, ".cc") == 0 || strcmp(lower, ".cxx") == 0) {
    return WB_FILE_ICON_CODE_C;
  }

  /* C/C++ header files */
  if (strcmp(lower, ".h") == 0 || strcmp(lower, ".hpp") == 0 ||
      strcmp(lower, ".hxx") == 0) {
    return WB_FILE_ICON_CODE_H;
  }

  /* Python */
  if (strcmp(lower, ".py") == 0 || strcmp(lower, ".pyw") == 0) {
    return WB_FILE_ICON_CODE_PY;
  }

  /* JavaScript/TypeScript */
  if (strcmp(lower, ".js") == 0 || strcmp(lower, ".jsx") == 0 ||
      strcmp(lower, ".ts") == 0 || strcmp(lower, ".tsx") == 0) {
    return WB_FILE_ICON_CODE_JS;
  }

  /* Other code files */
  if (strcmp(lower, ".java") == 0 || strcmp(lower, ".go") == 0 ||
      strcmp(lower, ".rs") == 0 || strcmp(lower, ".rb") == 0 ||
      strcmp(lower, ".php") == 0 || strcmp(lower, ".sh") == 0 ||
      strcmp(lower, ".bash") == 0 || strcmp(lower, ".lua") == 0 ||
      strcmp(lower, ".pl") == 0 || strcmp(lower, ".html") == 0 ||
      strcmp(lower, ".css") == 0 || strcmp(lower, ".xml") == 0 ||
      strcmp(lower, ".sql") == 0 || strcmp(lower, ".asm") == 0 ||
      strcmp(lower, ".s") == 0) {
    return WB_FILE_ICON_CODE_OTHER;
  }

  /* Images */
  if (strcmp(lower, ".png") == 0 || strcmp(lower, ".jpg") == 0 ||
      strcmp(lower, ".jpeg") == 0 || strcmp(lower, ".gif") == 0 ||
      strcmp(lower, ".bmp") == 0 || strcmp(lower, ".svg") == 0 ||
      strcmp(lower, ".webp") == 0 || strcmp(lower, ".ico") == 0 ||
      strcmp(lower, ".tiff") == 0) {
    return WB_FILE_ICON_IMAGE;
  }

  /* Documents */
  if (strcmp(lower, ".pdf") == 0 || strcmp(lower, ".doc") == 0 ||
      strcmp(lower, ".docx") == 0 || strcmp(lower, ".xls") == 0 ||
      strcmp(lower, ".xlsx") == 0 || strcmp(lower, ".ppt") == 0 ||
      strcmp(lower, ".pptx") == 0 || strcmp(lower, ".odt") == 0 ||
      strcmp(lower, ".ods") == 0 || strcmp(lower, ".odp") == 0 ||
      strcmp(lower, ".txt") == 0 || strcmp(lower, ".rtf") == 0) {
    return WB_FILE_ICON_DOCUMENT;
  }

  /* Archives */
  if (strcmp(lower, ".zip") == 0 || strcmp(lower, ".tar") == 0 ||
      strcmp(lower, ".gz") == 0 || strcmp(lower, ".bz2") == 0 ||
      strcmp(lower, ".xz") == 0 || strcmp(lower, ".7z") == 0 ||
      strcmp(lower, ".rar") == 0 || strcmp(lower, ".deb") == 0 ||
      strcmp(lower, ".rpm") == 0) {
    return WB_FILE_ICON_ARCHIVE;
  }

  /* Audio */
  if (strcmp(lower, ".mp3") == 0 || strcmp(lower, ".wav") == 0 ||
      strcmp(lower, ".flac") == 0 || strcmp(lower, ".ogg") == 0 ||
      strcmp(lower, ".aac") == 0 || strcmp(lower, ".m4a") == 0) {
    return WB_FILE_ICON_AUDIO;
  }

  /* Video */
  if (strcmp(lower, ".mp4") == 0 || strcmp(lower, ".mkv") == 0 ||
      strcmp(lower, ".avi") == 0 || strcmp(lower, ".mov") == 0 ||
      strcmp(lower, ".webm") == 0 || strcmp(lower, ".flv") == 0) {
    return WB_FILE_ICON_VIDEO;
  }

  /* Markdown */
  if (strcmp(lower, ".md") == 0 || strcmp(lower, ".markdown") == 0) {
    return WB_FILE_ICON_MARKDOWN;
  }

  /* Config files */
  if (strcmp(lower, ".json") == 0 || strcmp(lower, ".yaml") == 0 ||
      strcmp(lower, ".yml") == 0 || strcmp(lower, ".toml") == 0 ||
      strcmp(lower, ".ini") == 0 || strcmp(lower, ".conf") == 0 ||
      strcmp(lower, ".cfg") == 0) {
    return WB_FILE_ICON_CONFIG;
  }

  return WB_FILE_ICON_FILE;
}

/* ===== Sorting ===== */

static sort_type g_sort_type = WB_SORT_BY_NAME;
static sort_order g_sort_order = WB_SORT_ASCENDING;

static int CompareEntries(const void *a, const void *b) {
  const fs_entry *ea = (const fs_entry *)a;
  const fs_entry *eb = (const fs_entry *)b;

  /* Directories first */
  if (ea->is_directory && !eb->is_directory)
    return -1;
  if (!ea->is_directory && eb->is_directory)
    return 1;

  /* ".." always first */
  if (strcmp(ea->name, "..") == 0)
    return -1;
  if (strcmp(eb->name, "..") == 0)
    return 1;

  int result = 0;
  switch (g_sort_type) {
  case WB_SORT_BY_NAME:
    result = strcasecmp(ea->name, eb->name);
    break;
  case WB_SORT_BY_SIZE:
    if (ea->size < eb->size)
      result = -1;
    else if (ea->size > eb->size)
      result = 1;
    else
      result = strcasecmp(ea->name, eb->name);
    break;
  case WB_SORT_BY_DATE:
    if (ea->modified_time < eb->modified_time)
      result = -1;
    else if (ea->modified_time > eb->modified_time)
      result = 1;
    else
      result = strcasecmp(ea->name, eb->name);
    break;
  }

  if (g_sort_order == WB_SORT_DESCENDING) {
    result = -result;
  }

  return result;
}

/* ===== Selection Helpers ===== */

/* Set bit in selection bitmask */
static void SetSelectionBit(fs_state *state, i32 index, b32 value) {
  if (index < 0 || index >= (i32)state->entry_count)
    return;
  i32 byte_idx = index / 8;
  i32 bit_idx = index % 8;
  if (value) {
    state->selected[byte_idx] |= (1 << bit_idx);
  } else {
    state->selected[byte_idx] &= ~(1 << bit_idx);
  }
}

static b32 GetSelectionBit(fs_state *state, i32 index) {
  if (index < 0 || index >= (i32)state->entry_count)
    return 0;
  i32 byte_idx = index / 8;
  i32 bit_idx = index % 8;
  return (state->selected[byte_idx] >> bit_idx) & 1;
}

/* ===== Core API ===== */

void FS_Init(fs_state *state, memory_arena *arena) {
  memset(state, 0, sizeof(*state));
  state->arena = arena;
  state->entry_capacity = FS_MAX_ENTRIES;
  state->entries = ArenaPushArray(arena, fs_entry, FS_MAX_ENTRIES);
  state->selected_index = 0;

  memset(state->selected, 0, sizeof(state->selected));
  state->selection_count = 0;
  state->selection_anchor = -1;

  state->sort_by = WB_SORT_BY_NAME;
  state->sort_dir = WB_SORT_ASCENDING;
}

b32 FS_LoadDirectory(fs_state *state, const char *path) {
  /* Resolve to absolute path */
  char resolved[FS_MAX_PATH];
  if (!Platform_GetRealPath(path, resolved, FS_MAX_PATH)) {
    /* If resolving fails, try using the path as-is */
    strncpy(resolved, path, FS_MAX_PATH - 1);
    resolved[FS_MAX_PATH - 1] = '\0';
  }
  FS_NormalizePath(resolved);

  temporary_memory temp = BeginTemporaryMemory(state->arena);

  /* Load directory listing from platform */
  directory_listing listing = {0};
  if (!Platform_ListDirectory(resolved, &listing, state->arena)) {
    EndTemporaryMemory(temp);
    return false;
  }

  /* Store current path and normalize it */
  strncpy(state->current_path, resolved, FS_MAX_PATH - 1);
  state->current_path[FS_MAX_PATH - 1] = '\0';
  FS_NormalizePath(state->current_path);

  /* Clear existing entries */
  state->entry_count = 0;

  /* Process entries from platform listing */
  for (usize i = 0;
       i < listing.count && state->entry_count < state->entry_capacity; i++) {
    file_info *info = &listing.entries[i];
    fs_entry *entry = &state->entries[state->entry_count];

    /* Copy name */
    strncpy(entry->name, info->name, FS_MAX_NAME - 1);
    entry->name[FS_MAX_NAME - 1] = '\0';

    /* Build full path */
    FS_JoinPath(entry->path, FS_MAX_PATH, resolved, info->name);

    /* Set properties */
    entry->is_directory = (info->type == WB_FILE_TYPE_DIRECTORY);
    entry->size = info->size;
    entry->modified_time = info->modified_time;

    /* Determine icon type */
    if (info->type == WB_FILE_TYPE_SYMLINK) {
      entry->icon = WB_FILE_ICON_SYMLINK;
    } else {
      entry->icon = FS_GetIconType(info->name, entry->is_directory);
    }

    state->entry_count++;
  }

  /* Sort entries */
  if (state->entry_count > 0) {
    g_sort_type = state->sort_by;
    g_sort_order = state->sort_dir;
    qsort(state->entries, state->entry_count, sizeof(fs_entry), CompareEntries);
  }

  /* Reset selection to first entry (after ..) or 0 */
  state->selected_index = 0;
  if (state->entry_count > 1 && strcmp(state->entries[0].name, "..") == 0) {
    state->selected_index = 1;
  }

  /* Clear multi-selection when loading new directory */
  FS_ClearSelection(state);
  state->selection_anchor = -1;

  /* Sync single selection with multi-selection */
  if (state->entry_count > 0) {
    SetSelectionBit(state, state->selected_index, 1);
    state->selection_count = 1;
  }

  Platform_FreeDirectoryListing(&listing);
  EndTemporaryMemory(temp);
  return true;
}

b32 FS_NavigateUp(fs_state *state) {
  /* Already at root? */
  if (strcmp(state->current_path, "/") == 0) {
    return false;
  }

  /* Find parent directory */
  char parent[FS_MAX_PATH];
  strncpy(parent, state->current_path, FS_MAX_PATH);
  parent[FS_MAX_PATH - 1] = '\0';

  const char *last_sep = FS_FindLastSeparator(parent);
  if (last_sep == parent) {
    /* Parent is root */
    parent[1] = '\0';
  } else if (last_sep) {
    /* Truncate at separator, but handle Windows drives like C:/ later if
     * needed. For now, just truncate. */
    char *sep_ptr = (char *)last_sep;
    *sep_ptr = '\0';
  }

  return FS_LoadDirectory(state, parent);
}

b32 FS_NavigateInto(fs_state *state) {
  fs_entry *entry = FS_GetSelectedEntry(state);
  if (!entry || !entry->is_directory) {
    return false;
  }

  /* Handle ".." */
  if (strcmp(entry->name, "..") == 0) {
    return FS_NavigateUp(state);
  }

  return FS_LoadDirectory(state, entry->path);
}

void FS_SetSortOptions(fs_state *state, sort_type type, sort_order order) {
  state->sort_by = type;
  state->sort_dir = order;
  FS_Resort(state);
}

void FS_Resort(fs_state *state) {
  if (state->entry_count > 0) {
    /* Remember currently selected entry name to restore selection after sort */
    char selected_name[FS_MAX_NAME] = {0};
    fs_entry *selected = FS_GetSelectedEntry(state);
    if (selected) {
      strncpy(selected_name, selected->name, sizeof(selected_name) - 1);
    }

    g_sort_type = state->sort_by;
    g_sort_order = state->sort_dir;
    qsort(state->entries, state->entry_count, sizeof(fs_entry), CompareEntries);

    /* Restore selection */
    if (selected_name[0]) {
      for (u32 i = 0; i < state->entry_count; i++) {
        if (strcmp(state->entries[i].name, selected_name) == 0) {
          state->selected_index = i;
          /* Sync multi-selection */
          FS_SelectSingle(state, i);
          break;
        }
      }
    }
  }
}

fs_entry *FS_GetSelectedEntry(fs_state *state) {
  if (state->selected_index < 0 ||
      (u32)state->selected_index >= state->entry_count) {
    return NULL;
  }
  return &state->entries[state->selected_index];
}

fs_entry *FS_GetEntry(fs_state *state, i32 index) {
  if (index < 0 || (u32)index >= state->entry_count) {
    return NULL;
  }
  return &state->entries[index];
}

void FS_SetSelection(fs_state *state, i32 index) {
  if (state->entry_count == 0) {
    state->selected_index = 0;
    FS_ClearSelection(state);
    return;
  }
  state->selected_index = Clamp(index, 0, (i32)state->entry_count - 1);

  /* Also update multi-selection to single item */
  FS_SelectSingle(state, state->selected_index);
}

void FS_MoveSelection(fs_state *state, i32 delta) {
  FS_SetSelection(state, state->selected_index + delta);
}

/* ===== Multi-Selection API Implementation ===== */

void FS_SelectSingle(fs_state *state, i32 index) {
  FS_ClearSelection(state);
  if (index >= 0 && index < (i32)state->entry_count) {
    SetSelectionBit(state, index, 1);
    state->selection_count = 1;
    state->selection_anchor = index;
    state->selected_index = index;
  }
}

void FS_SelectToggle(fs_state *state, i32 index) {
  if (index < 0 || index >= (i32)state->entry_count)
    return;

  b32 was_selected = GetSelectionBit(state, index);
  SetSelectionBit(state, index, !was_selected);

  if (was_selected) {
    state->selection_count--;
  } else {
    state->selection_count++;
    state->selection_anchor = index;
  }

  /* Update selected_index to last toggled on, or first selected if toggled off
   */
  if (!was_selected) {
    state->selected_index = index;
  } else if (state->selection_count > 0) {
    state->selected_index = FS_GetFirstSelected(state);
  } else {
    state->selected_index = -1;
  }
}

void FS_SelectRange(fs_state *state, i32 from, i32 to) {
  if (from > to) {
    i32 temp = from;
    from = to;
    to = temp;
  }

  /* Clear existing and select range */
  FS_ClearSelection(state);

  from = Max(0, from);
  to = Min(to, (i32)state->entry_count - 1);

  for (i32 i = from; i <= to; i++) {
    SetSelectionBit(state, i, 1);
  }

  state->selection_count = to - from + 1;
  state->selected_index = to;
}

void FS_SelectAll(fs_state *state) {
  for (u32 i = 0; i < state->entry_count; i++) {
    SetSelectionBit(state, i, 1);
  }
  state->selection_count = (i32)state->entry_count;
}

void FS_ClearSelection(fs_state *state) {
  memset(state->selected, 0, sizeof(state->selected));
  state->selection_count = 0;
}

b32 FS_IsSelected(fs_state *state, i32 index) {
  return GetSelectionBit(state, index);
}

i32 FS_GetSelectionCount(fs_state *state) { return state->selection_count; }

i32 FS_GetFirstSelected(fs_state *state) {
  for (u32 i = 0; i < state->entry_count; i++) {
    if (GetSelectionBit(state, i))
      return (i32)i;
  }
  return -1;
}

i32 FS_GetNextSelected(fs_state *state, i32 after) {
  for (i32 i = after + 1; i < (i32)state->entry_count; i++) {
    if (GetSelectionBit(state, i))
      return i;
  }
  return -1;
}

const char *FS_GetHomePath(void) {
  const char *home = getenv("HOME");
  if (home)
    return home;

#ifdef _WIN32
  return "C:/";
#else
  return "/";
#endif
}

b32 FS_NavigateHome(fs_state *state) {
  return FS_LoadDirectory(state, FS_GetHomePath());
}

/* ===== File Operations ===== */

b32 FS_Delete(const char *path, memory_arena *arena) {
  (void)arena; /* Unused, platform handles recursion if needed */
  return Platform_Delete(path);
}

b32 FS_Rename(const char *old_path, const char *new_path) {
  return Platform_Rename(old_path, new_path);
}

b32 FS_CreateDirectory(const char *path) {
  return Platform_CreateDirectory(path);
}

b32 FS_CreateFile(const char *path) { return Platform_CreateFile(path); }

b32 FS_Copy(const char *src, const char *dst) {
  return Platform_Copy(src, dst);
}

/* Helper: Recursively copy directory contents */
static b32 CopyDirectoryRecursive(const char *src_dir, const char *dst_dir, memory_arena *arena) {
  directory_listing listing = {0};
  temporary_memory temp = BeginTemporaryMemory(arena);
  
  if (!Platform_ListDirectory(src_dir, &listing, arena)) {
    EndTemporaryMemory(temp);
    return false;
  }
  
  b32 success = true;
  
  for (usize i = 0; i < listing.count && success; i++) {
    file_info *info = &listing.entries[i];
    
    /* Skip . and .. */
    if (strcmp(info->name, ".") == 0 || strcmp(info->name, "..") == 0) {
      continue;
    }
    
    /* Build source and destination paths */
    char src_path[FS_MAX_PATH];
    char dst_path[FS_MAX_PATH];
    FS_JoinPath(src_path, sizeof(src_path), src_dir, info->name);
    FS_JoinPath(dst_path, sizeof(dst_path), dst_dir, info->name);
    
    if (info->type == WB_FILE_TYPE_DIRECTORY) {
      /* Create destination directory */
      if (!Platform_CreateDirectory(dst_path)) {
        /* Directory might already exist, that's OK */
        if (!Platform_IsDirectory(dst_path)) {
          success = false;
          break;
        }
      }
      /* Recursively copy subdirectory */
      if (!CopyDirectoryRecursive(src_path, dst_path, arena)) {
        success = false;
        break;
      }
    } else {
      /* Copy file */
      if (!Platform_Copy(src_path, dst_path)) {
        success = false;
        break;
      }
    }
  }
  
  Platform_FreeDirectoryListing(&listing);
  EndTemporaryMemory(temp);
  return success;
}

b32 FS_CopyRecursive(const char *src, const char *dst, memory_arena *arena) {
  /* Check if source exists */
  file_info info;
  if (!Platform_GetFileInfo(src, &info)) {
    return false;
  }
  
  if (info.type == WB_FILE_TYPE_FILE) {
    /* Simple file copy */
    return Platform_Copy(src, dst);
  }
  
  if (info.type == WB_FILE_TYPE_DIRECTORY) {
    /* Create destination directory */
    if (!Platform_CreateDirectory(dst)) {
      /* Directory might already exist, that's OK for root of copy */
      if (!Platform_IsDirectory(dst)) {
        return false;
      }
    }
    
    /* Recursively copy contents */
    return CopyDirectoryRecursive(src, dst, arena);
  }
  
  /* Symlinks not supported for recursive copy */
  return false;
}

b32 FS_Exists(const char *path) { return Platform_FileExists(path); }

b32 FS_ResolvePath(const char *path, char *out_path, usize out_size) {
  if (!path || path[0] == '\0')
    return false;

  char expanded[FS_MAX_PATH];
  expanded[0] = '\0';

  /* Handle ~ expansion */
  if (path[0] == '~') {
    const char *home = FS_GetHomePath();
    if (path[1] == '/' || path[1] == '\\') {
      snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1);
    } else if (path[1] == '\0') {
      strncpy(expanded, home, sizeof(expanded) - 1);
      expanded[sizeof(expanded) - 1] = '\0';
    } else {
      /* Handles cases like ~user - not fully supported but we'll try as-is */
      strncpy(expanded, path, sizeof(expanded) - 1);
      expanded[sizeof(expanded) - 1] = '\0';
    }
  } else {
    strncpy(expanded, path, sizeof(expanded) - 1);
    expanded[sizeof(expanded) - 1] = '\0';
  }

  /* Use platform to get absolute/real path */
  if (Platform_GetRealPath(expanded, out_path, out_size)) {
    FS_NormalizePath(out_path);
    return true;
  }

  /* Fallback: just normalize the expanded path if realpath fails (e.g. doesn't
   * exist yet but we want to refer to it) */
  strncpy(out_path, expanded, out_size - 1);
  out_path[out_size - 1] = '\0';
  FS_NormalizePath(out_path);
  return true;
}

/* Helper: Check if path is absolute (platform-aware) */
static b32 IsAbsolutePath(const char *path) {
  if (!path || path[0] == '\0')
    return false;
  if (path[0] == '/')
    return true; /* Unix absolute */
#ifdef _WIN32
  /* Windows drive letter like C:\ or C:/ */
  if (path[1] == ':' && ((path[0] >= 'A' && path[0] <= 'Z') ||
                         (path[0] >= 'a' && path[0] <= 'z'))) {
    return true;
  }
  /* UNC path like \\server\share */
  if (path[0] == '\\' && path[1] == '\\')
    return true;
#endif
  return false;
}

b32 FS_FindDeepestValidDirectory(const char *path, char *out_dir,
                                  usize out_size) {
  if (!path || !out_dir || out_size == 0)
    return false;

  i32 len = (i32)strlen(path);
  if (len == 0)
    return false;

  /* Normalize path first */
  char normalized[FS_MAX_PATH];
  if ((usize)len >= sizeof(normalized))
    len = (i32)sizeof(normalized) - 1;
  memcpy(normalized, path, len);
  normalized[len] = '\0';
  FS_NormalizePath(normalized);
  len = (i32)strlen(normalized);

  /* Start with root if path is absolute */
  char deepest_valid[FS_MAX_PATH] = {0};
  i32 i = 0;

  if (IsAbsolutePath(normalized)) {
    if (normalized[0] == '/') {
      /* Unix root */
      deepest_valid[0] = '/';
      deepest_valid[1] = '\0';
      i = 1;
    }
#ifdef _WIN32
    else if (normalized[1] == ':') {
      /* Windows drive root like C:/ */
      deepest_valid[0] = normalized[0];
      deepest_valid[1] = ':';
      deepest_valid[2] = '/';
      deepest_valid[3] = '\0';
      i = 3;
    }
#endif
  }

  /* Scan through path, validating each component */
  while (i < len) {
    /* Skip leading slashes */
    while (i < len && FS_IsPathSeparator(normalized[i]))
      i++;
    if (i >= len)
      break;

    /* Find end of this component */
    while (i < len && !FS_IsPathSeparator(normalized[i]))
      i++;

    /* Build path up to this component */
    char test_path[FS_MAX_PATH];
    i32 test_len = i;
    if (test_len >= FS_MAX_PATH)
      test_len = FS_MAX_PATH - 1;
    memcpy(test_path, normalized, test_len);
    test_path[test_len] = '\0';

    /* Check if this path exists and is a directory */
    if (Platform_IsDirectory(test_path)) {
      memcpy(deepest_valid, test_path, test_len + 1);
    } else {
      /* This component is invalid, stop here */
      break;
    }
  }

  /* Return the deepest valid directory we found */
  if (deepest_valid[0] != '\0') {
    strncpy(out_dir, deepest_valid, out_size - 1);
    out_dir[out_size - 1] = '\0';
    return true;
  }

  return false;
}
