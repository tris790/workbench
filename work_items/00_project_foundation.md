# Work Item 00: Project Foundation

## Objective
Set up the core project structure, build system, and platform abstraction layer.

## Requirements
- Create `build.sh` for compilation
- C99 only, handmade hero style
- No external dependencies beyond platform APIs
- Portable executable output

## Deliverables

### 1. Build System (`build.sh`)
- Single shell script that compiles the entire project
- Support debug and release builds via argument
- Output: `wb` (or `wb.exe` on Windows)

### 2. Platform Abstraction Layer (`platform.h` / `platform_*.c`)
- Window creation and management
- Event loop (keyboard, mouse, resize)
- File system operations (list dir, read file, get file info)
- Process spawning for terminal
- Clipboard access

### 3. Entry Point (`main.c`)
- Initialize platform
- Create main window
- Enter event loop
- Handle graceful shutdown

### 4. Core Types (`types.h`)
- Basic type definitions (u8, u32, i32, f32, etc.)
- Boolean, null, array macros
- Memory arena allocator basics

### 5. Documentation (`Gemini.md`)
- Coding style guide
- Naming conventions
- Project structure explanation

## Success Criteria
- `./build.sh` produces a working executable
- Window opens and responds to close event
- Clean compilation with no warnings
- Code follows handmade hero style

## Dependencies
None - this is the first work item.

## Next Work Item
01_rendering_system.md
