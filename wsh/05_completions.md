# WSH - Completions & Abbreviations Implementation Plan

## 1. Completions Engine

### Architecture
Triggered by `Tab`. Displays a pager (interactive list) below the command line.

### Data Sources
1.  **Paths**: Files and directories in `CWD`.
2.  **Commands**: Binaries in `PATH`.
3.  **Generated Completions**: Context-aware arguments derived from man pages or completion files.

### Man Page Parsing (The "Magic")
Fish parses man pages to generate completions automatically.

**Strategy for WSH:**
1.  **Background Worker**: When a command is first encountered or updated, fire a background thread/process.
2.  **Scan**: finding `man` page for command.
    *   Cygwin: `/usr/share/man`
3.  **Parse**: Read `troff` or `nroff` output.
    *   Regex search for lines starting with `-` (flags).
    *   Extract descriptions.
4.  **Cache**: Save generated completion rules to `~/.local/state/wsh/completions/cmd_name.wsh_comp`.

**Example Parsed Object**:
```json
{
  "command": "git",
  "flags": [
    {"short": "-v", "long": "--verbose", "desc": "Be verbose"},
    {"short": "-C", "desc": "Run as if started in <path>"}
  ]
}
```

## 2. Pager Interface
*   **Grid View**: If many short options, display in grid.
*   **List View**: If options have descriptions, display list with description on right.
*   **Navigation**: `Arrow Keys`, `Tab` (cycle), `Shift+Tab`.
*   **Search**: Typing while pager is open filters the list.

## 3. Abbreviations (`abbr`)

### Implementation
*   **Definition**: Stored in a hash map `shortcuts -> expansion`.
*   **Trigger**: Spacebar or Enter.
*   **Action**:
    1.  Check if current word matches an abbreviation key.
    2.  If yes, replace the text in buffer with the value.
    3.  Move cursor accordingly.
*   **Feedback**: The user sees the expansions happen in real-time.

### Storage
*   Persisted in config `~/.config/wsh/config.wsh` or specialized `abbr` file.

## 4. Universal Variables
*   **Command**: `set -U name value`.
*   **Mechanism**:
    *   Stored in a dedicated file `~/.config/wsh/fish_variables` (or similar).
    *   On start, shell loads these.
    *   File monitoring (inotify/Windows equivalents) to reload if changed by another shell instance.
    *   **IPC**: Inter-Process Communication might be used to notify other instances immediately (optional).
