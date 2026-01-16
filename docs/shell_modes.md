# Workbench Shell (WSH) Architecture & Platform Integration

This document outlines the shell operation modes and platform-specific implementation details for the integrated terminal in Workbench.

## Shell Operation Modes

Defined in `src/ui/components/terminal_panel.h`, the `ShellMode` enum determines how the terminal interacts with the underlying operating system:

| Mode | Identifier | Description |
| :--- | :--- | :--- |
| **Native WSH** | `SHELL_WSH` | Uses the built-in Workbench Shell. Provides the most advanced features, including real-time syntax highlighting, man-page completions, and abbreviations. |
| **Emulated** | `SHELL_EMULATED` | Uses the system default shell but augments it with a basic suggestion engine for path and history completion. |
| **System** | `SHELL_SYSTEM` | Spawns the system default shell (e.g., `bash`, `zsh`, or `cmd.exe`) directly with no additional Workbench intelligence. |
| **Fish** | `SHELL_FISH` | Specifically targets the `fish` shell if available on the system. |

## Platform Implementation Matrix

| Feature | Linux | Windows |
| :--- | :--- | :--- |
| **Default Mode** | `SHELL_WSH` | `SHELL_WSH` |
| **PTY Backend** | `forkpty()` (POSIX) | **ConPTY** (Windows Pseudo Console) |
| **Binary Name** | `wsh` | `wsh.exe` |
| **Binary Search** | Relative to executable path | Relative to executable path |
| **Shell Fallback** | `getenv("SHELL")` or `/bin/sh` | `cmd.exe` |
| **Cygwin Support** | N/A | Full (via `/dev/clipboard` path logic) |

## Key Technical Details

### 1. Unified Intelligence
Workbench is transitioning away from `SHELL_EMULATED` towards full `SHELL_WSH` dominance. In the emulated mode, suggestions are handled by `src/terminal/suggestion.c` in the UI thread. In `SHELL_WSH`, all shell intelligence (highlighting, completion, abbreviations) is handled within the `wsh` binary itself, reducing complexity in the main application.

### 2. Binary Resolution
The application uses `Platform_GetExecutablePath` to resolve the location of the `wsh` binary at runtime. This ensures that the terminal can always find the shell even if the application is launched from a different working directory.
- **Linux Strategy**: Replaces the main executable name with `wsh`.
- **Windows Strategy**: Replaces the main executable name with `wsh.exe`.

### 3. Windows/Cygwin Integration
The Windows implementation of `wsh` is designed to be "Cygwin-aware." It leverages ConPTY for rendering but hooks into Cygwin-specific paths (like `/dev/clipboard`) when a Cygwin environment is detected, providing a seamless Unix-like experience on Windows without sacrificing native performance.

### 4. Terminal Capabilities
The terminal emulator (`src/terminal/terminal.c`) supports a wide range of ANSI/VT100 escape sequences, including:
- **SGR (Select Graphic Rendition)**: 16-color, 256-color, and some 24-bit color support.
- **CSI sequences**: For cursor manipulation, screen clearing, and scroll margin management.
- **OSC 7**: Working directory synchronization (allowing the UI to track the shell's CWD).
