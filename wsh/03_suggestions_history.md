# WSH - Autosuggestions & History Implementation Plan

## 1. Intelligent Autosuggestions (The "Psychic" Feature)

### Concept
Suggestions appear to the right of the cursor in a dimmed color (grey). They are not part of the buffer until accepted.

### Sources
1.  **Command History**: Most recent commands matching the current prefix.
2.  **Directory History**: Paths visited frequently (for `cd`).
3.  **Man Page Completions** (fallback): If no history match, suggest flags/subcommands from man pages.

### Logic
1.  **Trigger**: On every keystroke that changes the buffer.
2.  **Query**: Search the history database for entries starting with `Buffer.text`.
3.  **Selection**: 
    *   Prioritize exact prefix matches.
    *   Prioritize most recently used (MRU) or most frequently used (MFU).
4.  **Display**:
    *   Render the buffer text normally.
    *   Render the remainder of the suggestion in `GREY` (ANSI `\x1b[90m` or `\x1b[38;5;240m`).

### Implementation Details
*   **Data Structure**: Trie or simple reverse-search list for history. Since history can be large, an index might be needed for performance.
*   **Async**: If history is huge, search shouldn't block typing. (Though typically memory-mapped history file is fast enough).

## 2. History Management

### Storage Format
We will use a human-readable, robust format similar to Fish or YAML to allow manual editing and recovery.

**File**: `~/.local/share/wsh/wsh_history`

**Format Example (YAML-like)**:
```yaml
- cmd: git commit -m "update"
  when: 1678889900
  paths:
    - /home/user/repo
- cmd: cd wsh
  when: 1678889920
```

### Features
*   **De-duplication**: Don't save consecutive duplicates.
*   **Universal**: Shared across sessions immediately (append-only file with file locking or atomic writes).

## 3. Interactive History Search
*   **Action**: User types `gi` and presses `Up Arrow`.
*   **Behavior**:
    *   Shell searches history for commands simply *starting* with `gi`.
    *   Displays matches one by one or in a dropdown if configured.
    *   Cursor stays at end of line.
*   **Difference from Autosuggestion**: Suggestion shows *one* ghost prediction. History search iterates through *all* matches.

## Windows/Cygwin Specifics
*   **Paths in History**: Store paths as POSIX paths (`/cygdrive/c/...`) to maintain portability within the Cygwin environment.
