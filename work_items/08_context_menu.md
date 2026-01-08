# Work Item 08: Context Menu

## Objective
Implement a non-bloated, customizable context menu system.

## Requirements
- Right-click or keyboard shortcut to open
- Clean, minimal design
- Customizable via settings
- Keyboard navigation within menu

## Deliverables

### 1. Context Menu Core (`context_menu.h` / `context_menu.c`)
```c
typedef struct {
    String label;
    String shortcut;
    Icon icon;
    CommandFn action;
    bool separator_after;
    bool enabled;
} MenuItem;

typedef struct {
    MenuItem *items;
    u32 item_count;
    Vec2 position;
    i32 selected_index;
    bool visible;
} ContextMenu;
```

### 2. Menu Rendering
- Appears at cursor position
- Shadow/depth effect
- Smooth fade in
- Correct positioning (flip if near edge)
- Nested submenus support

### 3. Menu Interactions
- Click to select
- Hover highlight
- Arrow keys to navigate
- Enter to execute
- Escape to close
- Click outside to close

### 4. Context-Aware Menus
- File context (on file)
- Directory context (on folder)
- Empty space context
- Selection context (multiple items)

### 5. Default Menu Items

**File Context:**
- Open
- Open With...
- Copy
- Cut
- Rename
- Delete
- Copy Path
- Properties

**Directory Context:**
- Open
- Open in New Panel
- Open in Terminal
- Copy
- Cut
- Delete
- Copy Path

### 6. Customization
- Add/remove items via settings
- Custom commands in context menu
- Separator control

## Success Criteria
- Menu appears at correct position
- Keyboard navigation works
- All default actions work
- Clean, non-bloated appearance
- Custom items can be added

## Dependencies
- 07_preview_panel.md

## Next Work Item
09_settings.md
