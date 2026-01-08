# Work Item 02: UI Framework

## Objective
Create the immediate mode UI framework for all interactive elements with keyboard-first design.

## Requirements
- Keyboard focused but mouse supported
- Immediate mode architecture
- Smooth user experience
- Professional feel

## Deliverables

### 1. UI Core (`ui.h` / `ui.c`)
- UI context and state management
- Hot/active element tracking
- Focus management
- Layout system (flex-like)

### 2. Input Handling
- Keyboard navigation (Tab, arrows, Enter, Esc)
- Mouse click, hover, drag
- Scroll (mouse wheel, keyboard)
- Keyboard shortcuts

### 3. Basic Widgets
```c
bool ui_button(String label);
bool ui_text_input(String *value, String placeholder);
void ui_label(String text);
bool ui_selectable(String label, bool selected);
void ui_separator(void);
```

### 4. Layout Containers
```c
void ui_begin_horizontal(void);
void ui_end_horizontal(void);
void ui_begin_vertical(void);
void ui_end_vertical(void);
void ui_begin_scroll(Vec2 size);
void ui_end_scroll(void);
```

### 5. Focus & Navigation System
- Tab order
- Focus rings
- Keyboard-driven list navigation
- Search/filter in lists

## Success Criteria
- All UI can be operated via keyboard alone
- Mouse interactions feel natural
- Focus is clearly visible
- Smooth scrolling
- UI is composable (base widget can be composed into more complex widgets)
- No layout glitches

## Dependencies
- 01_rendering_system.md

## Next Work Item
03_file_explorer.md
