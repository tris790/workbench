# Work Item 07: Preview Panel

## Objective
Implement file preview capabilities for various file types.

## Requirements
- Quick preview without opening external app
- Text file preview with syntax highlighting
- Image preview
- Streaming for large files

## Deliverables

### 1. Preview System (`preview.h` / `preview.c`)
```c
typedef enum {
    PREVIEW_NONE,
    PREVIEW_TEXT,
    PREVIEW_IMAGE,
    PREVIEW_BINARY,
    PREVIEW_DIRECTORY,
} PreviewType;

typedef struct {
    PreviewType type;
    String path;
    union {
        TextPreview text;
        ImagePreview image;
        // ...
    };
} Preview;
```

### 2. Text Preview
- Syntax highlighting for common languages
- Line numbers
- Scroll support
- Word wrap toggle
- First N lines for large files (lazy load more)

### 3. Image Preview
- Common formats (PNG, JPG, GIF, BMP)
- Fit to panel
- Zoom controls
- Image dimensions display

### 4. Directory Preview
- File count
- Total size
- Quick stats

### 5. Binary Preview
- Hex view
- File size and type info
- First N bytes

### 6. Syntax Highlighting (`syntax.h` / `syntax.c`)
- Simple regex-based highlighting
- Languages: C, Python, JavaScript, JSON, Markdown, Shell
- Keywords, strings, comments, numbers
- Extensible via config

### 7. Preview Toggle
- Spacebar to quick preview (modal)
- Side panel preview mode
- Auto-preview on selection (configurable)

## Success Criteria
- Preview loads quickly (<200ms)
- Text is readable with proper colors
- Images display correctly
- Large files don't freeze UI
- Toggle works smoothly

## Dependencies
- 06_terminal.md

## Next Work Item
08_context_menu.md
