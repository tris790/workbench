#!/bin/bash
set -e

# ensure build dir
mkdir -p build

echo "Cross-compiling for Windows using Zig..."

# Source files
SOURCES="
src/main.c
src/commands.c
src/core/animation.c
src/core/fs.c
src/core/fuzzy_match.c
src/core/input.c
src/core/key_repeat.c
src/core/text.c
src/core/theme.c
src/platform/windows/windows_platform.c
src/platform/windows/windows_window.c
src/platform/windows/windows_events.c
src/platform/windows/windows_filesystem.c
src/platform/windows/windows_clipboard.c
src/platform/windows/windows_process.c
src/platform/windows/windows_time.c
src/platform/windows/windows_fs_watcher.c
src/platform/windows/windows_font.c
src/renderer/renderer_software.c
src/renderer/icons.c
src/terminal/ansi_parser.c
src/terminal/command_history.c
src/terminal/suggestion.c
src/terminal/terminal.c
src/platform/windows/windows_pty.c
src/ui/ui.c
src/ui/layout.c
src/ui/components/breadcrumb.c
src/ui/components/command_palette.c
src/ui/components/context_menu.c
src/ui/components/dialog.c
src/ui/components/explorer.c
src/ui/components/file_item.c
src/ui/components/quick_filter.c
src/ui/components/terminal_panel.c
"

# Includes
INCLUDES="-Isrc -Isrc/core -Isrc/platform -Isrc/renderer -Isrc/ui -Isrc/ui/components -Isrc/terminal"

# Flags
# -static to avoid dependency on libgcc/libstdc++ dlls if using mingw
CFLAGS="-target x86_64-windows-gnu -std=c99 -g -gcodeview -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -DWB_DEBUG"
LIBS="-luser32 -lgdi32 -lshell32 -lshlwapi -ldwrite -luuid"

# Compile
# We use zig cc as a drop-in replacement for gcc/clang
zig cc $CFLAGS $INCLUDES $SOURCES $LIBS -o build/wb.exe

echo "Build complete: build/wb.exe"
echo "To run with wine: wine build/wb.exe"
