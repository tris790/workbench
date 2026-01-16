# WSH - Windows & Cygwin Specific Integration

## Overview
While the shell is cross-platform, the Windows experience (via Cygwin) requires specific attention to bridge the gap between Unix-like shell expectations and the Windows host OS.

## 1. Cygwin Environment Assurances
*   **Root**: `/cygdrive/c` maps to `C:\`.
*   **Home**: `/home/user` (Cygwin home) != `C:\Users\User` (Windows Home). WSH should probably respect Cygwin's home for config to keep it self-contained, but might want to alias `~win` to Windows home for convenience.
*   **Permissions**: `chmod` on Cygwin is an emulation. We generally respect it but knowing it maps to complex Windows ACLs.

## 2. Clipboard Integration
Terminal clipboard ergonomics are vital.
*   **Copy (Ctrl+C handling)**: In shell mode (no command running), Ctrl+C cancels line. If text selected (mouse), terminal handles it.
*   **System Integration**:
    *   `wsh` should provide `pbcopy` / `pbpaste` equivalents if not present.
    *   Utilize `/dev/clipboard` provided by Cygwin for reading/writing logic internally if needed (e.g. for scripted copy).

## 3. Terminal Capability (termcap)
*   We rely on **Windows Terminal**, **Mintty**, or **WezTerm** to be the host.
*   WSH will output standard ANSI/VT100 sequences.
*   **Color Support**: Detect 24-bit color support (`$COLORTERM` = `truecolor`) and use modern palettes.

## 4. Executable Handling
*   **Extension Agnostic**: Typing `code` should find `code.cmd` or `code.exe` in PATH if `code` (no extension) isn't found.
*   **Shebang parsing**: If executing a text file with `#!/bin/wsh`, handle it correctly.

## 5. Startup Performance
Cygwin process spawning (`fork`) is notoriously slower than Linux.
*   **Optimization**: Minimize subshells in startup scripts.
*   **Autoloading**: Lazy load completion functions and heavyweight configuration only when needed.

## 6. Installation Strategy
*   WSH is a binary compiled inside Cygwin.
*   User adds line to `/etc/nsswitch.conf` or `/etc/passwd` (Cygwin) to make WSH their default shell.
*   Or, creating a Windows Shortcut: `C:\cygwin64\bin\mintty.exe -i /Cygwin-Terminal.ico /bin/wsh.exe -l`
