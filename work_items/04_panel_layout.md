# Work Item 04: Panel Layout System

## Objective
Implement the 1 or 2 panel layout system with smooth transitions and resizable dividers.

## Requirements
- Support single panel mode
- Support dual panel mode (side by side)
- Toggle via command palette
- Resizable divider
- Smooth transition animations

## Deliverables

### 1. Layout Manager (`layout.h` / `layout.c`)
```c
typedef enum {
    LAYOUT_SINGLE,
    LAYOUT_DUAL,
} LayoutMode;

typedef struct {
    LayoutMode mode;
    f32 split_ratio;      // 0.0-1.0, position of divider
    f32 target_ratio;     // for animation
    u32 active_panel;     // 0 or 1
} Layout;
```

### 2. Panel Container
- Each panel is independent file explorer instance
- Active panel has visual indicator
- Tab/Ctrl+Tab to switch panels
- Panel-specific CWD

### 3. Divider
- Draggable with mouse
- Double-click to reset to 50/50
- Minimum panel width enforced
- Visual feedback on hover/drag

### 4. Layout Commands
- `Toggle Dual Panel` - switch between 1 and 2 panels
- `Focus Left Panel` / `Focus Right Panel`
- `Swap Panels` - swap contents
- `Equalize Panels` - set 50/50 split

### 5. Animations
- Smooth panel resize when toggling modes
- Divider drag responsiveness
- Focus transition

## Success Criteria
- Seamless toggle between single/dual mode
- Divider drag feels responsive
- Each panel maintains its own state
- Keyboard navigation between panels works
- No layout jank during transitions

## Dependencies
- 03_file_explorer.md

## Next Work Item
05_command_palette.md
