# Work Item 03: File Explorer

## Objective
Implement the core file explorer panel with directory listing, navigation, and file operations.

## Requirements
- Fast directory listing
- Icons for file types
- Keyboard navigation (j/k, arrows, enter, backspace)
- Current working directory tracking

## Deliverables

### 1. File System Model (`fs.h` / `fs.c`)
```c
typedef struct {
    String name;
    String path;
    bool is_directory;
    u64 size;
    u64 modified_time;
    FileType type;  // for icon selection
} FileEntry;

typedef struct {
    String current_path;
    FileEntry *entries;
    u32 entry_count;
    u32 selected_index;
} DirectoryState;
```

### 2. File Explorer Panel (`explorer.h` / `explorer.c`)
- Directory content listing
- File/folder icons
- Size and date columns (optional, toggleable)
- Selection highlighting
- Smooth scroll when navigating

### 3. Navigation
- Enter directory on Enter/double-click
- Go up with Backspace
- Breadcrumb path display
- Quick jump to home, root

### 4. File Type Icons
- Directory, file (generic)
- Code files (.c, .h, .py, .js, etc.)
- Images, documents, archives
- Executable
- Use simple geometric icons (not emojis)

### 5. File Operations (basic)
- Create file/folder
- Rename
- Delete (with confirmation)
- Copy/Cut/Paste

## Success Criteria
- Directory loads instantly (<100ms for 1000 files)
- Smooth keyboard navigation
- Correct icons for common file types
- File operations work correctly
- CWD is tracked accurately

## Dependencies
- 02_ui_framework.md

## Next Work Item
04_panel_layout.md
