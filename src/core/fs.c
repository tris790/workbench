/*
 * fs.c - File System Model Implementation
 *
 * C99, handmade hero style.
 */

#define _POSIX_C_SOURCE 200809L

#include "fs.h"
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
    return FILE_ICON_DIRECTORY;
  }

  const char *ext = FS_GetExtension(filename);
  if (!ext || ext[0] == '\0') {
    /* Check if executable */
    return FILE_ICON_FILE;
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
    return FILE_ICON_CODE_C;
  }

  /* C/C++ header files */
  if (strcmp(lower, ".h") == 0 || strcmp(lower, ".hpp") == 0 ||
      strcmp(lower, ".hxx") == 0) {
    return FILE_ICON_CODE_H;
  }

  /* Python */
  if (strcmp(lower, ".py") == 0 || strcmp(lower, ".pyw") == 0) {
    return FILE_ICON_CODE_PY;
  }

  /* JavaScript/TypeScript */
  if (strcmp(lower, ".js") == 0 || strcmp(lower, ".jsx") == 0 ||
      strcmp(lower, ".ts") == 0 || strcmp(lower, ".tsx") == 0) {
    return FILE_ICON_CODE_JS;
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
    return FILE_ICON_CODE_OTHER;
  }

  /* Images */
  if (strcmp(lower, ".png") == 0 || strcmp(lower, ".jpg") == 0 ||
      strcmp(lower, ".jpeg") == 0 || strcmp(lower, ".gif") == 0 ||
      strcmp(lower, ".bmp") == 0 || strcmp(lower, ".svg") == 0 ||
      strcmp(lower, ".webp") == 0 || strcmp(lower, ".ico") == 0 ||
      strcmp(lower, ".tiff") == 0) {
    return FILE_ICON_IMAGE;
  }

  /* Documents */
  if (strcmp(lower, ".pdf") == 0 || strcmp(lower, ".doc") == 0 ||
      strcmp(lower, ".docx") == 0 || strcmp(lower, ".xls") == 0 ||
      strcmp(lower, ".xlsx") == 0 || strcmp(lower, ".ppt") == 0 ||
      strcmp(lower, ".pptx") == 0 || strcmp(lower, ".odt") == 0 ||
      strcmp(lower, ".ods") == 0 || strcmp(lower, ".odp") == 0 ||
      strcmp(lower, ".txt") == 0 || strcmp(lower, ".rtf") == 0) {
    return FILE_ICON_DOCUMENT;
  }

  /* Archives */
  if (strcmp(lower, ".zip") == 0 || strcmp(lower, ".tar") == 0 ||
      strcmp(lower, ".gz") == 0 || strcmp(lower, ".bz2") == 0 ||
      strcmp(lower, ".xz") == 0 || strcmp(lower, ".7z") == 0 ||
      strcmp(lower, ".rar") == 0 || strcmp(lower, ".deb") == 0 ||
      strcmp(lower, ".rpm") == 0) {
    return FILE_ICON_ARCHIVE;
  }

  /* Audio */
  if (strcmp(lower, ".mp3") == 0 || strcmp(lower, ".wav") == 0 ||
      strcmp(lower, ".flac") == 0 || strcmp(lower, ".ogg") == 0 ||
      strcmp(lower, ".aac") == 0 || strcmp(lower, ".m4a") == 0) {
    return FILE_ICON_AUDIO;
  }

  /* Video */
  if (strcmp(lower, ".mp4") == 0 || strcmp(lower, ".mkv") == 0 ||
      strcmp(lower, ".avi") == 0 || strcmp(lower, ".mov") == 0 ||
      strcmp(lower, ".webm") == 0 || strcmp(lower, ".flv") == 0) {
    return FILE_ICON_VIDEO;
  }

  /* Markdown */
  if (strcmp(lower, ".md") == 0 || strcmp(lower, ".markdown") == 0) {
    return FILE_ICON_MARKDOWN;
  }

  /* Config files */
  if (strcmp(lower, ".json") == 0 || strcmp(lower, ".yaml") == 0 ||
      strcmp(lower, ".yml") == 0 || strcmp(lower, ".toml") == 0 ||
      strcmp(lower, ".ini") == 0 || strcmp(lower, ".conf") == 0 ||
      strcmp(lower, ".cfg") == 0) {
    return FILE_ICON_CONFIG;
  }

  return FILE_ICON_FILE;
}

/* ===== Sorting ===== */

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

  /* Then alphabetically (case-insensitive) */
  return strcasecmp(ea->name, eb->name);
}

/* ===== Core API ===== */

void FS_Init(fs_state *state, memory_arena *arena) {
  memset(state, 0, sizeof(*state));
  state->arena = arena;
  state->entry_capacity = FS_MAX_ENTRIES;
  state->entries = ArenaPushArray(arena, fs_entry, FS_MAX_ENTRIES);
  state->selected_index = 0;
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

  /* Create temporary memory scope for listing */
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
    entry->is_directory = (info->type == FILE_TYPE_DIRECTORY);
    entry->size = info->size;
    entry->modified_time = info->modified_time;

    /* Determine icon type */
    if (info->type == FILE_TYPE_SYMLINK) {
      entry->icon = FILE_ICON_SYMLINK;
    } else {
      entry->icon = FS_GetIconType(info->name, entry->is_directory);
    }

    state->entry_count++;
  }

  /* Sort entries */
  if (state->entry_count > 0) {
    qsort(state->entries, state->entry_count, sizeof(fs_entry), CompareEntries);
  }

  /* Reset selection to first entry (after ..) or 0 */
  state->selected_index = 0;
  if (state->entry_count > 1 && strcmp(state->entries[0].name, "..") == 0) {
    state->selected_index = 1;
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
    return;
  }
  if (index < 0)
    index = 0;
  if ((u32)index >= state->entry_count)
    index = (i32)state->entry_count - 1;
  state->selected_index = index;
}

void FS_MoveSelection(fs_state *state, i32 delta) {
  FS_SetSelection(state, state->selected_index + delta);
}

const char *FS_GetCurrentPath(fs_state *state) { return state->current_path; }

const char *FS_GetHomePath(void) {
  const char *home = getenv("HOME");
  return home ? home : "/";
}

b32 FS_NavigateHome(fs_state *state) {
  return FS_LoadDirectory(state, FS_GetHomePath());
}

/* ===== File Operations ===== */

b32 FS_Delete(const char *path) { return Platform_Delete(path); }

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
