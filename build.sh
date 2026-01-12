#!/bin/bash
#
# build.sh - Build script for Workbench
#
# Usage: ./build.sh [debug|release]
# Default: debug
#

set -e

# Build mode (default to debug)
BUILD_MODE="${1:-debug}"

# Output binary name
OUTPUT="build/wb"

# Ensure build directory exists
mkdir -p build

# Source files (new structure)
SOURCES="
    src/main.c
    src/platform/linux/linux_platform.c
    src/platform/linux/linux_window.c
    src/platform/linux/linux_events.c
    src/platform/linux/linux_filesystem.c
    src/platform/linux/linux_clipboard.c
    src/platform/linux/linux_process.c
    src/platform/linux/linux_time.c
    src/platform/protocols/xdg-shell-protocol.c
    src/platform/protocols/xdg-decoration-protocol.c
    src/renderer/renderer_software.c
    src/renderer/font.c
    src/renderer/icons.c
    src/core/theme.c
    src/core/animation.c
    src/core/fs.c
    src/core/input.c
    src/core/key_repeat.c
    src/core/text.c
    src/platform/linux/linux_fs_watcher.c
    src/ui/ui.c
    src/ui/layout.c
    src/ui/components/explorer.c
    src/ui/components/dialog.c
    src/ui/components/breadcrumb.c
    src/ui/components/file_item.c
    src/ui/components/command_palette.c
    src/ui/components/terminal_panel.c
    src/ui/components/quick_filter.c
    src/ui/components/context_menu.c
    src/core/fuzzy_match.c
    src/commands.c
    src/platform/linux/linux_pty.c
    src/terminal/terminal.c
    src/terminal/ansi_parser.c
    src/terminal/command_history.c
    src/terminal/suggestion.c
"

# Include paths for all source directories
INCLUDES="-Isrc -Isrc/core -Isrc/platform -Isrc/platform/protocols -Isrc/renderer -Isrc/ui -Isrc/ui/components -Isrc/terminal"

# Compiler and flags
CC="gcc"
CFLAGS="-std=c99 -D_GNU_SOURCE -Wall -Wextra -Werror -Wpedantic"
CFLAGS="$CFLAGS $INCLUDES"
CFLAGS="$CFLAGS $(pkg-config --cflags freetype2 fontconfig)"
LDFLAGS="-lwayland-client -lrt -lm -lutil -lpthread $(pkg-config --libs freetype2 fontconfig)"

# Mode-specific flags
if [ "$BUILD_MODE" = "release" ]; then
    echo "Building in RELEASE mode..."
    CFLAGS="$CFLAGS -O2 -DNDEBUG"
else
    echo "Building in DEBUG mode..."
    CFLAGS="$CFLAGS -O0 -g -DWB_DEBUG"
fi

# Generate xdg-shell protocol code if needed
XDG_SHELL_XML="/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml"
PROTO_DIR="src/platform/protocols"
if [ ! -f "$PROTO_DIR/xdg-shell-client-protocol.h" ] || [ "$XDG_SHELL_XML" -nt "$PROTO_DIR/xdg-shell-client-protocol.h" ]; then
    echo "Generating xdg-shell protocol code..."
    wayland-scanner client-header "$XDG_SHELL_XML" "$PROTO_DIR/xdg-shell-client-protocol.h"
    wayland-scanner private-code "$XDG_SHELL_XML" "$PROTO_DIR/xdg-shell-protocol.c"
fi

# Compile
echo "Compiling: $SOURCES"
$CC $CFLAGS $SOURCES -o $OUTPUT $LDFLAGS

echo "Build complete: ./$OUTPUT"
