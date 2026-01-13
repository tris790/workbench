# Workbench - Gemini AI Coding Guide

This document serves as the coding style guide and project documentation for Workbench (wb).

## Project Overview

Workbench is a developer tool that serves as a **file explorer on steroids** - a keyboard-focused, professional-grade file management application with integrated terminal, command palette, and extensibility.

### Target Platforms
- **Primary**: Arch Linux + KDE Wayland (native, no XWayland)
- **Future**: Windows (via platform abstraction, work item 11)

### Core Philosophy
- **Insanely fast and lightweight** - no bloat, portable single executable
- **Keyboard-focused** but mouse is supported
- **Professional UI** with smooth animations and delightful UX
- **Dark theme** by default, but themeable
- **Extensible** via user-defined commands
- **No hacks** - we take the time to do it right

---

## Coding Style

### Language & Standard
- **C99 only** (not C++ unless absolutely necessary)
- **Handmade Hero style** - direct, low-level, performance-oriented
- No external dependencies beyond platform APIs

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Types (structs, enums) | `snake_case` | `memory_arena`, `file_info` |
| Functions | `PascalCase` | `Platform_CreateWindow`, `App_Update` |
| Variables | `camelCase` or `snake_case` | `windowWidth`, `file_count` |
| Constants/Macros | `UPPER_SNAKE_CASE` | `MAX_PATH_LENGTH`, `MOD_CTRL` |

### Function Prefixes

| Prefix | Purpose |
|--------|---------|
| `Platform_` | Platform abstraction layer |
| `App_` | Application-level logic |
| `UI_` | User interface components |
| `Render_` | Rendering functions |

### File Organization

```
workbench/
├── build.sh              # Single build script
├── build/                # Build output (gitignored)
├── src/
│   ├── main.c            # Entry point
│   ├── core/             # Core utilities
│   │   ├── types.h       # Basic types, macros, memory arena
│   │   ├── animation.c/h # Animation system
│   │   ├── theme.c/h     # Theme/color system
│   │   └── fs.c/h        # Filesystem model
│   ├── platform/         # Platform abstraction
│   │   ├── platform.h    # Platform API declarations
│   │   ├── platform_linux.c
│   │   └── protocols/    # Wayland protocol files
│   ├── renderer/         # Rendering system
│   │   ├── renderer.h    # Renderer API
│   │   ├── renderer_software.c
│   │   ├── font.c/h      # Font rendering
│   │   └── icons.c/h     # Icon rendering
│   └── ui/               # UI system
│       ├── ui.c/h        # UI framework
│       └── components/   # UI components
│           └── explorer.c/h
└── Gemini.md             # This file
```

---

## Memory Management

All memory allocations go through **arena allocators**:

```c
memory_arena arena;
ArenaInit(&arena, buffer, Megabytes(64));

file_info *files = ArenaPushArray(&arena, file_info, count);
```

### Rules
1. No direct `malloc`/`free` in application code
2. Long-lived allocations use persistent arena
3. Frame/temporary allocations use scratch arena that's cleared each frame

---

## Platform Abstraction

The platform layer provides:
- Window creation and management
- Event loop (keyboard, mouse, resize)
- File system operations
- Clipboard access
- Process spawning (for terminal)
- Time functions

Platform implementations:
- `platform_linux.c` - Native Wayland using xdg-shell
- `platform_win32.c` - Win32 API (future)

### Important
- Linux uses **native Wayland** - no X11, no XWayland
- The platform layer isolates all OS-specific code

---

## Build System

Single script: `./build.sh [debug|release]`

- Default/preferred: debug mode (`-O0 -g -DWB_DEBUG`)
- Release: optimized (`-O2 -DNDEBUG`)
- Zero warnings policy (`-Wall -Wextra -Werror`)

---

## Design Guidelines

### UI/UX Principles
1. **Smooth animations** - all transitions should be animated
2. **Responsive** - instant feedback, never laggy
3. **Minimal but complete** - no bloat, but all needed features
4. **Consistent** - same patterns throughout
5. **Dark theme** - easy on the eyes, professional look

### Visual Design
- **Icon**: Use the custom workbench icon for the application.
- **Use icons** for compactness (not emojis - they look cheap)
- System fonts for native feel
- Command palette inspired by VSCode

### Keyboard Focus
- All actions accessible via keyboard
- Command palette for search and commands (prefix `>` for commands)
- Vim-style navigation where appropriate

---

## Feature Checklist

- [x] Project foundation (types, platform, build)
- [x] Rendering system
- [X] UI framework
- [X] File explorer
- [ ] Panel layout (1 or 2 panels)
- [ ] Command palette
- [ ] Built-in terminal
- [ ] Preview panel
- [ ] Context menu
- [ ] Settings
- [ ] Themes
- [ ] Windows support
