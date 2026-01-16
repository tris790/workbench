# WSH Code Review Phase 3: Intelligence

## Focus Areas
- Tab completion and man-page parsing.
- Command history management.
- Real-time syntax highlighting.
- Abbreviations and universal variables.

## Files to Review
- `src/shell/wsh_completion.h`
- `src/shell/wsh_completion.c`
- `src/shell/wsh_history.h`
- `src/shell/wsh_history.c`
- `src/shell/wsh_highlight.h`
- `src/shell/wsh_highlight.c`
- `src/shell/wsh_abbr.h`
- `src/shell/wsh_abbr.c`

## Review Observations

### Completion System
- [x] Supports three types of completion: Paths, Executables in `PATH`, and Command Flags.
- [x] **Innovative**: `scan_man_page` parses `man` output to dynamically generate flag completions.
- [x] Pager UI is integrated directly into the line editor with support for descriptions.
- [x] `scan_man_page`: Now uses `WSH_PAL_MkdirRecursive` which is portable and safer.
- [x] `complete_commands`: Now uses `WSH_PAL_GetPathSeparator()` for platform-aware `PATH` parsing.

### History Management
- [x] YAML-inspired format is easy to read and debug.
- [x] Basic de-duplication prevents history bloat from repeated commands.
- [x] Prefix-based autosuggestions (Ghost text) are implemented correctly.
- [w] History file loading doesn't handle very large files (reads everything into memory).

### Syntax Highlighting
- [x] Uses the real tokenizer to ensure highlighting matches shell interpretation.
- [x] Visual feedback for command existence (Red for unknown, Blue for valid).
- [x] Underlines valid paths found on disk.
- [x] `check_path_executable`: Now uses `WSH_PAL_GetPathSeparator()`.
- [x] `WSH_Highlight`: Refactored to use a dynamic growing buffer (StringBuilder approach) to avoid large fixed-overhead allocations.

## Suggested Improvements
1. [x] **Platform-Aware Path Parsing**: Created a helper in `WSH_PAL` and integrated it.
2. [x] **Safe Dir Creation**: Implemented `WSH_PAL_MkdirRecursive` and replaced `system`.
3. [x] **Optimized Highlighting**: Implemented dynamic buffer for highlighting.
4. [x] **History Rotation**: Implemented a 1000-entry limit for history in memory.
