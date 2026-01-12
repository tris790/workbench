/*
 * linux_filesystem.c - Linux file system operations
 */

#include "linux_internal.h"
#include <dirent.h>
#include <sys/stat.h>

/* ===== File System API ===== */

b32 Platform_ListDirectory(const char *path, directory_listing *listing,
                           memory_arena *arena) {
  DIR *dir = opendir(path);
  if (!dir)
    return false;

  listing->count = 0;
  listing->capacity = 2048;
  listing->entries = ArenaPushArray(arena, file_info, listing->capacity);

  if (!listing->entries) {
    closedir(dir);
    return false;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0)
      continue;

    if (listing->count >= listing->capacity)
      break;

    file_info *info = &listing->entries[listing->count];
    memset(info, 0, sizeof(file_info));

    strncpy(info->name, entry->d_name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode))
        info->type = FILE_TYPE_DIRECTORY;
      else if (S_ISLNK(st.st_mode))
        info->type = FILE_TYPE_SYMLINK;
      else
        info->type = FILE_TYPE_FILE;
      info->size = st.st_size;
      info->modified_time = st.st_mtime;
    } else {
      info->type = FILE_TYPE_FILE;
    }

    listing->count++;
  }

  closedir(dir);
  return true;
}

void Platform_FreeDirectoryListing(directory_listing *listing) {
  (void)listing;
  /* Arena-allocated, no free needed */
}

u8 *Platform_ReadEntireFile(const char *path, usize *out_size,
                            memory_arena *arena) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  usize size = ftell(file);
  fseek(file, 0, SEEK_SET);

  u8 *data = ArenaPushArray(arena, u8, size + 1);
  fread(data, 1, size, file);
  data[size] = '\0';

  fclose(file);

  if (out_size)
    *out_size = size;
  return data;
}

b32 Platform_GetFileInfo(const char *path, file_info *info) {
  struct stat st;
  if (stat(path, &st) != 0)
    return false;

  const char *name = strrchr(path, '/');
  name = name ? name + 1 : path;
  strncpy(info->name, name, sizeof(info->name) - 1);

  if (S_ISDIR(st.st_mode))
    info->type = FILE_TYPE_DIRECTORY;
  else if (S_ISLNK(st.st_mode))
    info->type = FILE_TYPE_SYMLINK;
  else
    info->type = FILE_TYPE_FILE;

  info->size = st.st_size;
  info->modified_time = st.st_mtime;

  return true;
}

b32 Platform_FileExists(const char *path) { return access(path, F_OK) == 0; }

b32 Platform_IsDirectory(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  return S_ISDIR(st.st_mode);
}

void Platform_OpenFile(const char *path) {
  char command[2048];
  /* Use xdg-open to open file with default application */
  /* Redirect output to silence spurious messages */
  snprintf(command, sizeof(command), "xdg-open \"%s\" > /dev/null 2>&1 &",
           path);
  int result = system(command);
  (void)result;
}
