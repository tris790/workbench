# Work Item 06: Built-in Terminal

## Objective
Implement an integrated terminal that stays in sync with the file explorer's current directory.

## Requirements

### Core Requirements
- CWD synced with file explorer
- Full terminal emulation (PTY)
- Keyboard input passthrough
- ANSI color support
- Resize support
- Terminal spawns in the same working directory as the split

### Platform Requirements
- **Platform-agnostic architecture**: Must be designed to run on Windows 11 and Arch Linux (Wayland) (implement linux right now)
- Abstract platform-specific code behind a common interface
- Linux: Use PTY via `forkpty()` or `openpty()`
- Windows: Use ConPTY (Windows Pseudo Console API)

### Shell Considerations
- Default shell should provide a pleasant experience out-of-the-box
- On Linux: fish shell is the gold standard for defaults and behavior (autosuggestions, syntax highlighting, smart completions)
- On Windows: **Custom fish-like shell as default**
  - PowerShell is too slow to boot (~500ms+) and has awkward syntax
  - **Don't require 100% PowerShell feature parity** — we're a dev tool, not a sysadmin console
  - Implement a fast, built-in shell with fish-like UX:
    - Instant startup
    - Autosuggestions from history
    - Syntax highlighting for commands
    - Smart tab completions
    - Common aliases (`ls`, `cat`, `rm`, `mkdir`, `mv`, `cp`, etc.)
  - For `.ps1` scripts or PowerShell-specific needs:
    - Spawn PowerShell as a **background process** (`powershell.exe -File <script>`)
    - Capture output and display in terminal
    - User can also explicitly run `pwsh <command>` to shell out
  - Alternatively, detect and use nushell/fish if installed on Windows
- Shell preference should be configurable per-platform

### Code Organization
- **Keep code well modularized by feature**
- Each major component in its own file/module
- Clear separation between:
  - Platform-specific PTY code (`pty_linux.c`, `pty_windows.c`)
  - Terminal emulation logic (`terminal.c`)
  - ANSI parsing (`ansi_parser.c`)
  - UI rendering (`terminal_panel.c`)
  - Command registration (`terminal_commands.c`)

## Deliverables

### 1. PTY Layer (`pty.h` / `pty_linux.c` / `pty_windows.c`)
- Platform-specific pseudo-terminal creation
- Read/write to terminal process
- Signal handling (resize)
- Process lifecycle management
- Common interface:
```c
typedef struct PTY PTY;
PTY* PTY_Create(const char *shell, const char *cwd);
void PTY_Destroy(PTY *pty);
i32  PTY_Read(PTY *pty, char *buffer, u32 size);
i32  PTY_Write(PTY *pty, const char *data, u32 size);
void PTY_Resize(PTY *pty, u32 cols, u32 rows);
bool PTY_IsAlive(PTY *pty);
```

### 2. Built-in Shell for Windows (`shell_builtin.h` / `shell_builtin.c`)
Fish-like shell providing instant startup and modern UX without PowerShell dependency:
```c
typedef struct BuiltinShell BuiltinShell;

// Initialization
BuiltinShell* Shell_Create(const char *cwd);
void Shell_Destroy(BuiltinShell *shell);

// Execute command, returns output
ShellResult Shell_Execute(BuiltinShell *shell, const char *command);

// Autosuggestion from history
const char* Shell_GetSuggestion(BuiltinShell *shell, const char *partial);

// Tab completion
char** Shell_GetCompletions(BuiltinShell *shell, const char *partial, u32 *count);
```

**Built-in commands:**
- `cd`, `pwd`, `ls`, `cat`, `head`, `tail`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `touch`
- `echo`, `clear`, `exit`, `history`, `env`, `export`, `which`, `type`
- `grep` (basic), `find` (basic), `wc`

**External command execution:**
- Parse command, check if built-in
- If not built-in: spawn via `CreateProcess()`, capture stdout/stderr
- `.ps1` files: automatically spawn `powershell.exe -File <script> <args>`
- Prefix command with `pwsh` to explicitly run in PowerShell

### 3. Terminal Emulator (`terminal.h` / `terminal.c`)
```c
typedef struct {
    char *buffer;        // screen buffer
    u32 cols, rows;
    u32 cursor_x, cursor_y;
    Color fg, bg;        // current colors
    // ... terminal state
} Terminal;
```

### 4. ANSI Parser (`ansi_parser.h` / `ansi_parser.c`)
- Basic ANSI escape sequences
- 16 colors minimum (256 nice to have)
- Cursor movement
- Clear screen/line
- Bold, underline, inverse

### 5. Terminal Panel (`terminal_panel.h` / `terminal_panel.c`)
- Toggle visibility (Ctrl+`)
- Appears at bottom of window
- Resizable height
- CWD label at top
- Scroll buffer

### 6. CWD Sync
- Terminal starts in explorer's CWD
- When explorer changes directory, terminal receives notification
- Option to auto-cd in terminal (configurable)
- **Split awareness**: New terminal instances inherit the working directory of the active split/panel

### 7. Terminal Commands (`terminal_commands.c`)
- Open Terminal (toggle visibility)
- Clear Terminal
- Kill Terminal Process
- New Terminal Instance
- Change Shell
- Split Terminal (horizontal/vertical)

## Success Criteria
- Linux: fish/bash/zsh run correctly with full PTY support
- Windows: Built-in fish-like shell starts instantly (<50ms)
- Windows: Autosuggestions and tab completion work smoothly
- Windows: Built-in commands (`ls`, `cd`, `cat`, etc.) work as expected
- Windows: External commands spawn correctly and capture output
- Windows: `.ps1` scripts run via background PowerShell process
- Colors render correctly
- Resize works without corruption
- Copy/paste works
- CWD sync works as expected
- Works on both Windows 11 and Arch Linux (Wayland)
- Terminal spawns in correct directory when split
- Code is modular and easy to maintain

## Dependencies
- 05_command_palette.md

## Next Work Item
07_preview_panel.md
