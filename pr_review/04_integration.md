# WSH Code Review Phase 4: Integration

## Focus Areas
- UI integration with the terminal panel.
- Build system updates (Linux & Windows).
- Main entry point logic.

## Files to Review
- `src/main.c`
- `src/ui/components/terminal_panel.h`
- `src/ui/components/terminal_panel.c`
- `build.sh`
- `crosscompile.sh`

## Review Observations

### UI Integration
- [x] `TerminalPanel` correctly spawns WSH binary from `./build/wsh`.
- [x] Input and ANSI rendering integration allows WSH to control the UI (Highlighting, Pager).
- [x] Cursor blinking and selection work as expected in the terminal panel.
- [x] **Overlap**: `terminal_panel.c` still contains an "EMULATED" shell mode with its own suggestion engine. This duplicate logic should be consolidated.
- [x] Hardcoded relative path to `./build/wsh` might fail if the app is launched from a different directory.

### Build & Platform Support
- [x] `build.sh` correctly builds the standalone WSH binary for Linux.
- [x] `crosscompile.sh` uses `zig cc` to successfully build `wsh.exe` for Windows.
- [x] Resource files (`.rc`) and icon integration are present in the Windows build.
- [x] WSH build currently lacks `-Werror` or strict optimization flags used in the main app build.

## Suggested Improvements
1. **Unify Shell Logic**: Phase out `SHELL_EMULATED` in `terminal_panel.c` or strictly define it as a fallback when WSH is missing.
2. **Binary Path Resolution**: Use a platform-specific way to find the WSH binary relative to the executable location instead of hardcoding `./build/wsh`.
3. **Build Consolidation**: Move WSH sources into a shared variable or include-file in the build scripts to ensure both app and shell use consistent flags.
4. **Main Font**: Consider loading a true monospace font for the terminal panel (currently it seems to use the main UI font which might be proportional).
