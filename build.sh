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

# Build and run the embedding tool (native C approach)
gcc -O2 scripts/embed.c -o build/embed
./build/embed

# Source files (Unity Build)
SOURCES="src/unity_linux.c"

# Include paths for all source directories
INCLUDES="-Isrc -Isrc/core -Isrc/platform -Isrc/platform/protocols -Isrc/renderer -Isrc/ui -Isrc/ui/components -Isrc/terminal -Isrc/config"

# Compiler and flags
CC="gcc"
CFLAGS="-std=c99 -D_GNU_SOURCE -Wall -Wextra -Werror -Wpedantic"
CFLAGS="$CFLAGS $INCLUDES"
CFLAGS="$CFLAGS $(pkg-config --cflags freetype2 fontconfig)"
LDFLAGS="-lwayland-cursor -lwayland-client -lwayland-egl -lEGL -lGL -lrt -lm -lutil -lpthread $(pkg-config --libs freetype2 fontconfig)"

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
echo "Compiling (unity build)..."
$CC $CFLAGS $SOURCES -o $OUTPUT $LDFLAGS

echo "Build complete: ./$OUTPUT"
