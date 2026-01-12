/*
 * windows_clipboard.c - Windows clipboard implementation
 *
 * Uses Win32 clipboard APIs with UTF-16 conversion.
 * C99, handmade hero style.
 */

#include "windows_internal.h"

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
