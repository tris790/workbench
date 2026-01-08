# Work Item 06: Built-in Terminal

## Objective
Implement an integrated terminal that stays in sync with the file explorer's current directory.

## Requirements
- CWD synced with file explorer
- Full terminal emulation (PTY)
- Keyboard input passthrough
- ANSI color support
- Resize support

## Deliverables

### 1. PTY Layer (`pty.h` / `pty_*.c`)
- Platform-specific pseudo-terminal creation
- Read/write to terminal process
- Signal handling (resize)
- Process lifecycle management

### 2. Terminal Emulator (`terminal.h` / `terminal.c`)
```c
typedef struct {
    char *buffer;        // screen buffer
    u32 cols, rows;
    u32 cursor_x, cursor_y;
    Color fg, bg;        // current colors
    // ... terminal state
} Terminal;
```

### 3. ANSI Parser
- Basic ANSI escape sequences
- 16 colors minimum (256 nice to have)
- Cursor movement
- Clear screen/line
- Bold, underline, inverse

### 4. Terminal Panel
- Toggle visibility (Ctrl+`)
- Appears at bottom of window
- Resizable height
- CWD label at top
- Scroll buffer

### 5. CWD Sync
- Terminal starts in explorer's CWD
- When explorer changes directory, terminal receives notification
- Option to auto-cd in terminal (configurable)

### 6. Terminal Commands
- Open Terminal (toggle visibility)
- Clear Terminal
- Kill Terminal Process
- New Terminal Instance

## Success Criteria
- Shell runs correctly (bash, zsh, fish)
- Colors render correctly
- Resize works without corruption
- Copy/paste works
- CWD sync works as expected

## Dependencies
- 05_command_palette.md

## Next Work Item
07_preview_panel.md
