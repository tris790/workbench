/*
 * windows_pty.c - Windows PTY implementation using ConPTY
 *
 * Spawns a shell in a pseudo-terminal using Windows Pseudo Console API.
 * C99, handmade hero style.
 */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include "../../terminal/workbench_pty.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <wchar.h>

/* ConPTY Definitions (if not available in MinGW headers) */
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef void *HPCON;

typedef HRESULT(WINAPI *PFN_CreatePseudoConsole)(COORD size, HANDLE hInput,
                                                 HANDLE hOutput, DWORD dwFlags,
                                                 HPCON *phPC);
typedef HRESULT(WINAPI *PFN_ResizePseudoConsole)(HPCON hPC, COORD size);
typedef void(WINAPI *PFN_ClosePseudoConsole)(HPCON hPC);

struct PTY {
  HANDLE hPipeIn;
  HANDLE hPipeOut;
  HPCON hPC;
  PROCESS_INFORMATION pi;
  PFN_ResizePseudoConsole fnResize;
  PFN_ClosePseudoConsole fnClose;
};

/* Global function pointers and probe state */
static PFN_CreatePseudoConsole s_fnCreate = NULL;
static PFN_ResizePseudoConsole s_fnResize = NULL;
static PFN_ClosePseudoConsole s_fnClose = NULL;
static b32 s_conpty_probed = false;

static void ProbeConPTY(void) {
  if (s_conpty_probed)
    return;

  /* kernel32.dll is always loaded in every Win32 process */
  HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
  if (hKernel32) {
    s_fnCreate = (PFN_CreatePseudoConsole)GetProcAddress(hKernel32,
                                                         "CreatePseudoConsole");
    s_fnResize = (PFN_ResizePseudoConsole)GetProcAddress(hKernel32,
                                                         "ResizePseudoConsole");
    s_fnClose =
        (PFN_ClosePseudoConsole)GetProcAddress(hKernel32, "ClosePseudoConsole");
  }

  s_conpty_probed = true;
}

PTY *PTY_Create(const char *shell, const char *cwd) {
  ProbeConPTY();

  if (!s_fnCreate || !s_fnResize || !s_fnClose) {
    /* ConPTY not supported on this Windows version (Needs Win10 1809+) */
    return NULL;
  }

  /* Manual memory management is acceptable in the platform layer */
  PTY *pty = malloc(sizeof(PTY));
  if (!pty)
    return NULL;
  memset(pty, 0, sizeof(PTY));
  pty->fnResize = s_fnResize;
  pty->fnClose = s_fnClose;

  HANDLE hPipePTYInSide, hPipePTYOutSide;
  HANDLE hPipeInSide, hPipeOutSide;

  if (!CreatePipe(&hPipePTYInSide, &hPipeInSide, NULL, 0)) {
    free(pty);
    return NULL;
  }
  if (!CreatePipe(&hPipeOutSide, &hPipePTYOutSide, NULL, 0)) {
    CloseHandle(hPipePTYInSide);
    CloseHandle(hPipeInSide);
    free(pty);
    return NULL;
  }

  /* Create ConPTY */
  COORD size = {80, 25};
  HRESULT hr = s_fnCreate(size, hPipePTYInSide, hPipePTYOutSide, 0, &pty->hPC);

  if (FAILED(hr)) {
    CloseHandle(hPipePTYInSide);
    CloseHandle(hPipePTYOutSide);
    CloseHandle(hPipeInSide);
    CloseHandle(hPipeOutSide);
    free(pty);
    return NULL;
  }

  /* Pipes for us to talk to PTY */
  pty->hPipeIn = hPipeInSide;   /* We write to this to send input to PTY */
  pty->hPipeOut = hPipeOutSide; /* We read from this to see PTY output */

  /* Close the handles that were passed to ConPTY, as it has its own
   * copies/ownership now */
  CloseHandle(hPipePTYInSide);
  CloseHandle(hPipePTYOutSide);

  /* Prepare Startup Info */
  STARTUPINFOEXW siEx;
  memset(&siEx, 0, sizeof(siEx));
  siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);

  size_t bytesRequired = 0;
  InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);
  siEx.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(bytesRequired);
  if (!siEx.lpAttributeList) {
    PTY_Destroy(pty);
    return NULL;
  }

  if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0,
                                         &bytesRequired)) {
    free(siEx.lpAttributeList);
    PTY_Destroy(pty);
    return NULL;
  }

  if (!UpdateProcThreadAttribute(siEx.lpAttributeList, 0,
                                 PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pty->hPC,
                                 sizeof(HPCON), NULL, NULL)) {
    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    free(siEx.lpAttributeList);
    PTY_Destroy(pty);
    return NULL;
  }

  /* Command Line */
  wchar_t wideCmdLine[FS_MAX_PATH];
  if (shell) {
    MultiByteToWideChar(CP_UTF8, 0, shell, -1, wideCmdLine, FS_MAX_PATH);
  } else {
    wcscpy(wideCmdLine, L"cmd.exe");
  }

  /* Working Directory */
  wchar_t wideCwd[FS_MAX_PATH];
  wchar_t *pWideCwd = NULL;
  if (cwd) {
    if (MultiByteToWideChar(CP_UTF8, 0, cwd, -1, wideCwd, FS_MAX_PATH) > 0) {
      /* Normalize slashes for Windows */
      for (wchar_t *p = wideCwd; *p; p++) {
        if (*p == L'/')
          *p = L'\\';
      }
      pWideCwd = wideCwd;
    }
  }

  /* Create Process */
  if (!CreateProcessW(NULL, wideCmdLine, NULL, NULL, FALSE,
                      EXTENDED_STARTUPINFO_PRESENT, NULL, pWideCwd,
                      &siEx.StartupInfo, &pty->pi)) {
    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    free(siEx.lpAttributeList);
    PTY_Destroy(pty);
    return NULL;
  }

  DeleteProcThreadAttributeList(siEx.lpAttributeList);
  free(siEx.lpAttributeList);
  return pty;
}

void PTY_Destroy(PTY *pty) {
  if (pty) {
    if (pty->fnClose && pty->hPC) {
      pty->fnClose(pty->hPC);
    }
    if (pty->pi.hProcess) {
      TerminateProcess(pty->pi.hProcess, 0);
      CloseHandle(pty->pi.hProcess);
    }
    if (pty->pi.hThread)
      CloseHandle(pty->pi.hThread);
    if (pty->hPipeIn)
      CloseHandle(pty->hPipeIn);
    if (pty->hPipeOut)
      CloseHandle(pty->hPipeOut);
    free(pty);
  }
}

i32 PTY_Read(PTY *pty, char *buffer, u32 size) {
  if (!pty || !pty->hPipeOut)
    return 0;

  DWORD bytesRead = 0;
  DWORD bytesAvail = 0;
  if (!PeekNamedPipe(pty->hPipeOut, NULL, 0, NULL, &bytesAvail, NULL)) {
    return -1;
  }

  if (bytesAvail == 0)
    return 0;

  if (ReadFile(pty->hPipeOut, buffer, size, &bytesRead, NULL)) {
    return (i32)bytesRead;
  }
  return 0;
}

i32 PTY_Write(PTY *pty, const char *data, u32 size) {
  if (!pty || !pty->hPipeIn)
    return 0;
  DWORD bytesWritten = 0;
  if (WriteFile(pty->hPipeIn, data, size, &bytesWritten, NULL)) {
    return (i32)bytesWritten;
  }
  return 0;
}

void PTY_Resize(PTY *pty, u32 cols, u32 rows) {
  if (!pty || !pty->fnResize || !pty->hPC)
    return;
  COORD s;
  s.X = (SHORT)cols;
  s.Y = (SHORT)rows;
  pty->fnResize(pty->hPC, s);
}

b32 PTY_IsAlive(PTY *pty) {
  if (!pty || !pty->pi.hProcess)
    return false;
  DWORD exitCode = 0;
  if (GetExitCodeProcess(pty->pi.hProcess, &exitCode)) {
    return exitCode == STILL_ACTIVE;
  }
  return false;
}

i32 PTY_GetFD(PTY *pty) {
  (void)pty;
  return -1; /* Not used on Windows */
}

#endif /* _WIN32 */
