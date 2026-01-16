# WSH (Workbench Shell) - Architecture & Core Implementation Plan

## Overview
WSH is a modern, Fish-inspired shell designed for cross-platform compatibility (Linux & Windows). It prioritizes user experience with "sane defaults," rich visual feedback, and intuitive interactions.

## Core Design Principles
1.  **Cross-Platform Abstraction**: Core logic uses a platform abstraction layer (PAL) to handle OS differences (Process creation, Signals, Filesystem).
2.  **Cygwin Synergy (Windows)**: On Windows, WSH assumes a Cygwin environment. This allows leveraging POSIX compatibility (paths like `/cygdrive/c`, standard tools `ls`, `grep`) while maintaining native Windows interoperability where needed.
3.  **Event-Driven Loop**: The main loop handles input, display updates, and asynchronous tasks (autosuggestion fetching) without blocking.

## Architecture Modules

### 1. State Management (`wsh_state`)
Holds global shell state:
*   Environment variables (`env_vars`)
*   Current working directory (`cwd`)
*   Command history (`history`)
*   Configuration (`config`)
*   Job control table (`jobs`)

### 2. Platform Abstraction Layer (`wsh_pal`)
Interfaces for OS-specific operations.

**Windows/Cygwin specifics:**
*   **Path Conversion**: normalize Windows paths (`C:\Users`) to Cygwin paths (`/cygdrive/c/Users`) for internal handling, and vice-versa when invoking native Windows executables.
*   **Process Execution**:
    *   Use `posix_spawn` or `fork/exec` where available (Cygwin).
    *   Handle `.exe` extensions implicitly.
    *   Environment block translation (Path vs PATH).

### 3. Read-Eval-Print Loop (REPL)
*   **Read**: `wsh_reader` - captures keys, handles line editing, handles signals (SIGINT).
*   **Eval**: `wsh_parser` -> `wsh_executor`.
    *   Parser produces an Abstract Syntax Tree (AST).
    *   Executor traverses AST and runs commands.
*   **Print**: `wsh_renderer` - handles syntax highlighting, phantom text, and cursor positioning.

## Windows Implementation Strategy (Cygwin Focus)

Since the user has Cygwin installed:
1.  **Compilation**: We target a Cygwin build (linking `cygwin1.dll`). This provides POSIX APIs (`fork`, `pipe`, `select`, `termios`).
2.  **Terminal Emulation**: We depend on the hosting terminal (e.g., Windows Terminal, Mintty) for VT100/ANSI support.
3.  **Clipboard**: Interop with Windows clipboard via `/dev/clipboard` or `clip.exe`.

### Path Handling
*   Internal representation: POSIX (`/home/user`, `/cygdrive/c/Project`).
*   External Execution:
    *   If executing a Cygwin binary: Pass POSIX paths.
    *   If executing a Win32 binary (e.g., `notepad.exe`): Convert paths to Win32 format using `cygwin_conv_path`.

## Build System
*   CMake based.
*   Detects `CYGWIN` environment to link appropriate libraries.
