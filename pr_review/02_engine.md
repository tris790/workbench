# WSH Code Review Phase 2: Engine

## Focus Areas
- Input handling and terminal raw mode.
- PTY integration and command execution.
- Line editing and escape sequence processing.

## Files to Review
- `src/shell/wsh_repl.h`
- `src/shell/wsh_repl.c`
- `src/shell/wsh_pal.h`
- `src/shell/wsh_pal.c`
- `src/platform/windows/windows_pty.c`

## Review Observations

### REPL & Line Editing
- [x] **CRITICAL**: `wsh_eval` uses `split_args` which ignores quotes/escapes. It should use the tokenizer/parser.
- [x] Input buffer correctly implements insertion, deletion, and word-kill (`Ctrl+W`).
- [x] Supports rich visual features: live syntax highlighting, autosuggestions, and a completion pager.
- [x] `REPL` loop uses blocking `read()`. This prevents background updates (e.g., async completion or time in prompt).
- [x] Escape sequence parsing assumes immediate arrival of sequence after `ESC`.

### Platform Abstraction (PAL)
- [x] Cross-platform execution logic (Fork/Exec on POSIX, Spawn on Windows).
- [x] Intelligent Windows extension probing (`.exe`, `.cmd`, `.bat`).
- [x] Builtin clipboard support for Cygwin via `/dev/clipboard`.
- [x] `WSH_PAL_ClipboardPaste`: `fseek` on `/dev/clipboard` may fail or return 0 on some systems. Better to read until EOF into a dynamic buffer.
- [x] `WSH_PAL_Execute`: Disabling raw mode before every command and re-enabling after is correct but might cause brief flicker or input loss if done too frequently.

### PTY & Execution
- [x] Excellent modern ConPTY implementation for Windows.
- [x] Correct use of `STARTUPINFOEXW` and attribute lists.
- [x] Proper UTF-8 to UTF-16 conversion for paths and commands.

## Suggested Improvements
1. **Integrate Parser**: Replace `split_args` in `wsh_eval` with `WSH_Parse` to support pipes, redirects, and quoted arguments. [DONE]
2. **Non-blocking Input**: Use `select()` or `poll()` in the REPL loop with a small timeout to allow for async UI updates. [DONE]
3. **Robust Paste**: Refactor `WSH_PAL_ClipboardPaste` to read into a growing buffer instead of relying on `fseek`. [DONE]
4. **ANSI Escape Parser**: Implement a more robust state-machine based escape sequence parser to handle partial reads or standalone `ESC` key. [DONE]
