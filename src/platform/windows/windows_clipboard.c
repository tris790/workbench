#ifdef _WIN32
/*
 * windows_clipboard.c - Windows clipboard implementation
 *
 * Uses Win32 clipboard APIs with UTF-16 conversion.
 * C99, handmade hero style.
 */

#include "windows_internal.h"
#include <shlobj.h>

/* ===== Clipboard API ===== */

char *Platform_GetClipboard(char *buffer, usize buffer_size) {
  if (!buffer || buffer_size == 0)
    return NULL;

  if (!OpenClipboard(NULL))
    return NULL;

  HANDLE data = GetClipboardData(CF_UNICODETEXT);
  if (!data) {
    CloseClipboard();
    return NULL;
  }

  wchar_t *wide_text = (wchar_t *)GlobalLock(data);
  if (!wide_text) {
    CloseClipboard();
    return NULL;
  }

  /* Convert UTF-16 to UTF-8 */
  int bytes_needed =
      WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, NULL, 0, NULL, NULL);
  if (bytes_needed > (int)buffer_size) {
    bytes_needed = (int)buffer_size;
  }

  WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, buffer, bytes_needed, NULL,
                      NULL);
  buffer[buffer_size - 1] = '\0';

  GlobalUnlock(data);
  CloseClipboard();

  return buffer;
}

b32 Platform_SetClipboard(const char *text) {
  if (!text)
    return false;

  if (!OpenClipboard(NULL))
    return false;

  EmptyClipboard();

  /* Calculate UTF-16 size */
  int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (wide_len <= 0) {
    CloseClipboard();
    return false;
  }

  /* Allocate global memory for clipboard */
  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wide_len * sizeof(wchar_t));
  if (!hMem) {
    CloseClipboard();
    return false;
  }

  wchar_t *wide_text = (wchar_t *)GlobalLock(hMem);
  if (!wide_text) {
    GlobalFree(hMem);
    CloseClipboard();
    return false;
  }

  MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, wide_len);
  GlobalUnlock(hMem);

  if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
    GlobalFree(hMem);
    CloseClipboard();
    return false;
  }

  /* Note: After SetClipboardData succeeds, the system owns hMem */
  CloseClipboard();
  return true;
}

/* ===== File Clipboard API ===== */

b32 Platform_ClipboardSetFiles(const char **paths, i32 count, b32 is_cut) {
  if (count <= 0 || count > 1024)
    return false;

  if (!OpenClipboard(NULL))
    return false;

  EmptyClipboard();

  /* Calculate total size needed for DROPFILES structure */
  /* DROPFILES header + all paths as wide chars + double null terminator */
  usize total_wide_chars = 0;
  wchar_t **wide_paths = (wchar_t **)_alloca(count * sizeof(wchar_t *));
  int *wide_lengths = (int *)_alloca(count * sizeof(int));

  for (i32 i = 0; i < count; i++) {
    int len = MultiByteToWideChar(CP_UTF8, 0, paths[i], -1, NULL, 0);
    wide_lengths[i] = len;
    wide_paths[i] = (wchar_t *)_alloca(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, paths[i], -1, wide_paths[i], len);
    total_wide_chars += len + 1; /* +1 for null between files */
  }
  total_wide_chars++; /* Final null for double-null termination */

  /* Allocate global memory for DROPFILES */
  usize total_size = sizeof(DROPFILES) + total_wide_chars * sizeof(wchar_t);
  HGLOBAL hDrop = GlobalAlloc(GMEM_MOVEABLE, total_size);
  if (!hDrop) {
    CloseClipboard();
    return false;
  }

  DROPFILES *df = (DROPFILES *)GlobalLock(hDrop);
  if (!df) {
    GlobalFree(hDrop);
    CloseClipboard();
    return false;
  }

  /* Fill DROPFILES structure */
  df->pFiles = sizeof(DROPFILES); /* Offset to file list */
  df->pt.x = 0;
  df->pt.y = 0;
  df->fNC = FALSE;
  df->fWide = TRUE; /* Using wide chars */

  /* Copy paths */
  wchar_t *dest = (wchar_t *)((BYTE *)df + sizeof(DROPFILES));
  for (i32 i = 0; i < count; i++) {
    memcpy(dest, wide_paths[i], wide_lengths[i] * sizeof(wchar_t));
    dest += wide_lengths[i];
    *dest++ = L'\0'; /* Null terminate each path */
  }
  *dest = L'\0'; /* Double null terminate */

  GlobalUnlock(hDrop);

  /* Set the HDROP data */
  if (!SetClipboardData(CF_HDROP, hDrop)) {
    GlobalFree(hDrop);
    CloseClipboard();
    return false;
  }

  /* Set preferred drop effect for cut vs copy */
  /* This tells Explorer whether to move or copy the files */
  HGLOBAL hEffect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (hEffect) {
    DWORD *effect = (DWORD *)GlobalLock(hEffect);
    if (effect) {
      *effect = is_cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
      GlobalUnlock(hEffect);
      
      /* Register CF_PREFERREDDROPEFFECT format if needed */
      static UINT cfPreferredDropEffect = 0;
      if (cfPreferredDropEffect == 0) {
        cfPreferredDropEffect = RegisterClipboardFormatA("Preferred DropEffect");
      }
      if (cfPreferredDropEffect != 0) {
        SetClipboardData(cfPreferredDropEffect, hEffect);
      } else {
        GlobalFree(hEffect);
      }
    } else {
      GlobalFree(hEffect);
    }
  }

  CloseClipboard();
  return true;
}

i32 Platform_ClipboardGetFiles(char **paths_out, i32 max_paths, b32 *is_cut_out) {
  *is_cut_out = false;

  if (!OpenClipboard(NULL))
    return 0;

  /* Check for HDROP data */
  HANDLE hDrop = GetClipboardData(CF_HDROP);
  if (!hDrop) {
    CloseClipboard();
    return 0;
  }

  HDROP drop = (HDROP)GlobalLock(hDrop);
  if (!drop) {
    CloseClipboard();
    return 0;
  }

  /* Get number of files */
  UINT file_count = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
  if (file_count > (UINT)max_paths) {
    file_count = max_paths;
  }

  /* Get each file path */
  wchar_t wide_path[FS_MAX_PATH];
  for (UINT i = 0; i < file_count; i++) {
    UINT len = DragQueryFileW(drop, i, wide_path, FS_MAX_PATH);
    if (len > 0) {
      WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, paths_out[i], FS_MAX_PATH,
                          NULL, NULL);
    }
  }

  GlobalUnlock(hDrop);

  /* Check for preferred drop effect (cut vs copy) */
  static UINT cfPreferredDropEffect = 0;
  if (cfPreferredDropEffect == 0) {
    cfPreferredDropEffect = RegisterClipboardFormatA("Preferred DropEffect");
  }

  if (cfPreferredDropEffect != 0) {
    HANDLE hEffect = GetClipboardData(cfPreferredDropEffect);
    if (hEffect) {
      DWORD *effect = (DWORD *)GlobalLock(hEffect);
      if (effect) {
        *is_cut_out = (*effect == DROPEFFECT_MOVE);
        GlobalUnlock(hEffect);
      }
    }
  }

  CloseClipboard();
  return (i32)file_count;
}
#endif /* _WIN32 */
