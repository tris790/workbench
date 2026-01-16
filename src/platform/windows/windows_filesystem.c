#ifdef _WIN32
/*
 * windows_filesystem.c - Windows file system operations
 *
 * Uses FindFirstFileW/FindNextFileW for directory listing,
 * and wide APIs with UTF-16 conversion.
 * C99, handmade hero style.
 */

#include "fs.h"
#include "windows_internal.h"
#include <shlwapi.h>

/* ===== UTF-8 / UTF-16 Conversion Helpers ===== */

static int Utf8ToWide(const char *utf8, wchar_t *wide, int wide_size) {
  if (!utf8 || !wide || wide_size <= 0)
    return 0;
  return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wide_size);
}

static int WideToUtf8(const wchar_t *wide, char *utf8, int utf8_size) {
  if (!wide || !utf8 || utf8_size <= 0)
    return 0;
  return WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8_size, NULL, NULL);
}

/* ===== File System API ===== */

b32 Platform_ListDirectory(const char *path, directory_listing *listing,
                           memory_arena *arena) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  wchar_t search_path[FS_MAX_PATH] = {0};

  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;

  /* Append \* for FindFirstFile search pattern */
  _snwprintf(search_path, FS_MAX_PATH - 1, L"%s\\*", wide_path);

  WIN32_FIND_DATAW find_data;
  HANDLE find_handle = FindFirstFileW(search_path, &find_data);

  if (find_handle == INVALID_HANDLE_VALUE)
    return false;

  listing->count = 0;
  listing->capacity = 2048;
  listing->entries = ArenaPushArray(arena, file_info, listing->capacity);

  if (!listing->entries) {
    FindClose(find_handle);
    return false;
  }

  do {
    /* Skip "." */
    if (wcscmp(find_data.cFileName, L".") == 0)
      continue;

    if (listing->count >= listing->capacity)
      break;

    file_info *info = &listing->entries[listing->count];
    memset(info, 0, sizeof(file_info));

    /* Convert filename to UTF-8 */
    if (WideToUtf8(find_data.cFileName, info->name, sizeof(info->name)) == 0) {
      /* Conversion failed (likely name too long or invalid chars) - skip entry
       */
      continue;
    }

    /* Skip entries with empty names or just dots (though . is skipped above, ..
     * might act weird if converted poorly) */
    if (info->name[0] == '\0') {
      continue;
    }

    /* Determine file type */
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      info->type = FILE_TYPE_DIRECTORY;
    } else if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      info->type = FILE_TYPE_SYMLINK;
    } else {
      info->type = FILE_TYPE_FILE;
    }

    /* File size (combine high and low parts) */
    info->size = ((u64)find_data.nFileSizeHigh << 32) | find_data.nFileSizeLow;

    /* Convert FILETIME to Unix timestamp */
    ULARGE_INTEGER ull;
    ull.LowPart = find_data.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = find_data.ftLastWriteTime.dwHighDateTime;
    /* FILETIME is 100-nanosecond intervals since Jan 1, 1601 */
    /* Unix epoch is Jan 1, 1970 - difference is 116444736000000000 */
    info->modified_time = (ull.QuadPart - 116444736000000000ULL) / 10000000ULL;

    listing->count++;
  } while (FindNextFileW(find_handle, &find_data));

  FindClose(find_handle);
  return true;
}

void Platform_FreeDirectoryListing(directory_listing *listing) {
  (void)listing;
  /* Arena-allocated, no free needed */
}

u8 *Platform_ReadEntireFile(const char *path, usize *out_size,
                            memory_arena *arena) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return NULL;

  HANDLE file = CreateFileW(wide_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return NULL;

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file, &file_size)) {
    CloseHandle(file);
    return NULL;
  }

  usize size = (usize)file_size.QuadPart;
  u8 *data = ArenaPushArray(arena, u8, size + 1);
  if (!data) {
    CloseHandle(file);
    return NULL;
  }

  DWORD bytes_read;
  if (!ReadFile(file, data, (DWORD)size, &bytes_read, NULL)) {
    CloseHandle(file);
    return NULL;
  }

  data[size] = '\0';
  CloseHandle(file);

  if (out_size)
    *out_size = size;
  return data;
}

b32 Platform_GetFileInfo(const char *path, file_info *info) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;

  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesExW(wide_path, GetFileExInfoStandard, &attr))
    return false;

  /* Extract filename from path */
  const char *name = FS_GetFilename(path);
  strncpy(info->name, name, sizeof(info->name) - 1);
  info->name[sizeof(info->name) - 1] = '\0';

  if (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    info->type = FILE_TYPE_DIRECTORY;
  } else if (attr.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    info->type = FILE_TYPE_SYMLINK;
  } else {
    info->type = FILE_TYPE_FILE;
  }

  info->size = ((u64)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;

  ULARGE_INTEGER ull;
  ull.LowPart = attr.ftLastWriteTime.dwLowDateTime;
  ull.HighPart = attr.ftLastWriteTime.dwHighDateTime;
  info->modified_time = (ull.QuadPart - 116444736000000000ULL) / 10000000ULL;

  return true;
}

b32 Platform_FileExists(const char *path) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;
  return GetFileAttributesW(wide_path) != INVALID_FILE_ATTRIBUTES;
}

b32 Platform_IsDirectory(const char *path) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;
  DWORD attr = GetFileAttributesW(wide_path);
  if (attr == INVALID_FILE_ATTRIBUTES)
    return false;
  return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void Platform_OpenFile(const char *path) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return;
  ShellExecuteW(NULL, L"open", wide_path, NULL, NULL, SW_SHOWNORMAL);
}

b32 Platform_CreateDirectory(const char *path) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;
  return CreateDirectoryW(wide_path, NULL) != 0;
}

b32 Platform_CreateFile(const char *path) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;

  HANDLE h = CreateFileW(wide_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(h);
  return true;
}

b32 Platform_Delete(const char *path) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;

  DWORD attrs = GetFileAttributesW(wide_path);
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return false;

  if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
    return RemoveDirectoryW(wide_path) != 0;
  } else {
    /* Clear read-only attribute if present to ensure deletion works */
    if (attrs & FILE_ATTRIBUTE_READONLY) {
      SetFileAttributesW(wide_path, attrs & ~FILE_ATTRIBUTE_READONLY);
    }
    return DeleteFileW(wide_path) != 0;
  }
}

b32 Platform_Rename(const char *old_path, const char *new_path) {
  wchar_t wide_old[FS_MAX_PATH] = {0};
  wchar_t wide_new[FS_MAX_PATH] = {0};
  if (Utf8ToWide(old_path, wide_old, FS_MAX_PATH) == 0)
    return false;
  if (Utf8ToWide(new_path, wide_new, FS_MAX_PATH) == 0)
    return false;

  return MoveFileW(wide_old, wide_new) != 0;
}

b32 Platform_Copy(const char *src, const char *dst) {
  wchar_t wide_src[FS_MAX_PATH] = {0};
  wchar_t wide_dst[FS_MAX_PATH] = {0};
  if (Utf8ToWide(src, wide_src, FS_MAX_PATH) == 0)
    return false;
  if (Utf8ToWide(dst, wide_dst, FS_MAX_PATH) == 0)
    return false;

  return CopyFileW(wide_src, wide_dst, FALSE) != 0;
}

b32 Platform_GetRealPath(const char *path, char *out_path, usize out_size) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  wchar_t wide_out[FS_MAX_PATH] = {0};

  if (Utf8ToWide(path, wide_path, FS_MAX_PATH) == 0)
    return false;

  DWORD result = GetFullPathNameW(wide_path, FS_MAX_PATH, wide_out, NULL);
  if (result == 0 || result > FS_MAX_PATH)
    return false;

  if (WideToUtf8(wide_out, out_path, (int)out_size) == 0)
    return false;

  return true;
}

b32 Platform_GetExecutablePath(char *out_path, usize out_size) {
  wchar_t wide_path[FS_MAX_PATH] = {0};
  DWORD result = GetModuleFileNameW(NULL, wide_path, FS_MAX_PATH);
  if (result == 0 || result == FS_MAX_PATH)
    return false;

  if (WideToUtf8(wide_path, out_path, (int)out_size) == 0)
    return false;

  return true;
}
#endif /* _WIN32 */
