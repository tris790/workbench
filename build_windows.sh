#!/bin/bash
#
# build_windows.sh - Build script for Workbench (Windows using Zig)
#
# Usage: ./build_windows.sh [debug|release]
# Default: debug
#

set -e

# Build mode (default to debug)
BUILD_MODE="${1:-debug}"

# Output binary name
OUTPUT="build/wb.exe"

# Ensure build directory exists
mkdir -p build

# Build and run the embedding tool
# Note: We use the host compiler for the embed tool, not the cross compiler
if command -v gcc &> /dev/null; then
    gcc -O2 scripts/embed.c -o build/embed
elif command -v zig &> /dev/null; then
    zig cc -O2 scripts/embed.c -o build/embed
else
    echo "Error: No C compiler found for embed tool (tried gcc, zig cc)"
    exit 1
fi
./build/embed

# Source files (Unity Build)
SOURCES="src/unity_windows.c src/platform/windows/workbench.rc"

# Include paths for all source directories
INCLUDES="-Isrc -Isrc/core -Isrc/platform -Isrc/renderer -Isrc/ui -Isrc/ui/components -Isrc/terminal -Isrc/config"

# Compiler and flags
CC="zig cc"
CFLAGS="-target x86_64-windows-gnu -std=c99 -Wall -Wextra -Werror"
CFLAGS="$CFLAGS -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS"
# Suppress warnings for Windows-specific code patterns
# -Wunknown-pragmas: #pragma comment(lib) is MSVC-specific
# -Wunused-parameter: COM interface callbacks have unused parameters
# -Wcast-function-type-mismatch: GetProcAddress returns generic function pointer
# -Wmissing-field-initializers: MONITORINFO initialization pattern
# -Wunused-but-set-variable: frame_count debug variable in main.c
CFLAGS="$CFLAGS -Wno-unknown-pragmas -Wno-unused-parameter -Wno-cast-function-type-mismatch -Wno-missing-field-initializers -Wno-unused-but-set-variable -Wno-unused-function"
CFLAGS="$CFLAGS $INCLUDES"

# Libraries
LDFLAGS="-luser32 -lgdi32 -lshell32 -lshlwapi -ldwrite -luuid -lole32 -lopengl32"

# Mode-specific flags
if [ "$BUILD_MODE" = "release" ]; then
    echo "Building in RELEASE mode..."
    CFLAGS="$CFLAGS -O2 -DNDEBUG"
else
    echo "Building in DEBUG mode..."
    CFLAGS="$CFLAGS -O0 -g -gcodeview -DWB_DEBUG"
fi

# WINDOWS subsystem - no console window on launch
LDFLAGS="$LDFLAGS -Wl,--subsystem,windows"

# Compile
echo "Compiling (unity build with zig cc)..."
$CC $CFLAGS $SOURCES -o $OUTPUT $LDFLAGS

echo "Build complete: ./$OUTPUT"
if command -v wine &> /dev/null; then
    echo "To run with wine: wine ./$OUTPUT"
fi
