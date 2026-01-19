# Work Item 14: Drag and Drop for File/Folder Management

## Objective
Implement an intuitive, professional, and visually elegant drag-and-drop system for moving files and folders within the file explorer. The feature should support **multi-selection** (dragging multiple files/folders at once), work seamlessly across split views, and provide clear visual feedback about what is being dragged and where it will be dropped.

---

## Requirements

### Core Functionality
1. **Multi-select files and folders** - Ctrl+Click to toggle selection, Shift+Click for range select
2. **Drag single or multiple items** from the file explorer list
3. **Drop into folders** to move the item(s) to that destination (can be empty space in file explorer or a folder)
4. **Cross-panel support** - drag from one split panel to another
5. **Cancel drag** - pressing Escape or releasing outside valid targets cancels the operation
6. **Prevent invalid operations** - cannot drop a folder into itself or its descendants
7. **Single click** - changes the selection

### Visual Design Goals
- **Beautiful and comfy** - smooth animations, subtle shadows, elegant transitions
- **Intuitive** - immediately clear what's being dragged and where it goes
- **Professional** - VSCode/modern IDE quality visuals
- **Responsive** - real-time feedback during drag operation
- **Multi-item preview** - stacked preview showing count when dragging multiple items

---

## Visual Design Specification

### Drag Preview (Ghost Image)
When dragging begins, show a **floating preview** that follows the cursor:

#### Single Item:
```
┌─────────────────────────────────┐
│  [Icon]  filename.txt           │  ← Semi-transparent (60% opacity)
│          └── 1.2 KB             │  ← Subtle file info
└─────────────────────────────────┘
       ↑ Offset from cursor (8-12px down-right)
```

#### Multiple Items (stacked effect):
```
    ┌─────────────────────────────────┐
   ┌┴────────────────────────────────┐│   ← Stacked cards effect
  ┌┴────────────────────────────────┐││
  │  [Icon]  filename.txt           │┘│   ← Top item visible
  │          └── +3 more items      │─┘   ← Count badge
  └─────────────────────────────────┘
                              ┌─────┐
                              │  4  │  ← Optional: count badge
                              └─────┘
```

**Properties:**
- **Opacity**: 0.6-0.7 (semi-transparent)
- **Scale**: Slightly smaller than original (0.9x)
- **Shadow**: Soft drop shadow for depth
- **Border**: Subtle accent color border
- **Animation**: Smooth pickup (scale from 1.0 to 0.9 over 100ms)
- **Offset**: 12px right, 12px down from cursor hotspot
- **Multi-item**: 2-3px offset between stacked cards, max 3 visible

### Drop Zone Highlighting

#### Valid Drop Target (Folder)
When hovering over a valid folder target:

```
┌─────────────────────────────────────────┐
│ ┌─────────────────────────────────────┐ │
│ │ [📁] target_folder/                 │ │  ← Highlighted with accent color
│ │      └── 5 items                    │ │  ← Pulsing glow animation
│ └─────────────────────────────────────┘ │
│                                         │
│   Drop here to move into folder         │  ← Optional tooltip
└─────────────────────────────────────────┘
```

**Properties:**
- **Background**: Accent color at 15-20% opacity
- **Border**: 2px accent color, solid
- **Animation**: Subtle pulse (opacity oscillates 15-25%)
- **Icon change**: Folder icon could animate open slightly

#### Invalid Drop Target
When hovering over an invalid target (file, same item, parent of dragged folder):

```
┌─────────────────────────────────────────┐
│ [🚫] Cannot drop here                   │  ← Red tint or no-drop cursor
└─────────────────────────────────────────┘
```

**Properties:**
- **Cursor**: Platform "no-drop" cursor
- **Visual**: Slight red tint or no highlight
- **No border highlight**

### Cross-Panel Drag Indicator
When dragging across split views:

```
┌───────────────────┐ │ ┌───────────────────┐
│  Panel A          │ │ │  Panel B          │
│                   │ │ │                   │
│  [Dragging from]  │ │ │  [Drop zone]      │
│                   │ │ │  ┌─────────────┐  │
│                   │ │ │  │ 📁 target/  │←─┤─ Highlighted
│                   │ │ │  └─────────────┘  │
└───────────────────┘ │ └───────────────────┘
         ↑ Source panel shows                ↑ Target panel shows
           subtle "source" indicator           active drop zones
```

### Drop Insertion Indicator
For dropping into the current directory (not into a subfolder), show an **insertion line**:

```
┌──────────────────────────────────────────┐
│  📁  folder_a/                           │
├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤  ← Insertion line (accent color, animated)
│  📄  file.txt   (being dragged)          │
│  📁  folder_b/                           │
└──────────────────────────────────────────┘
```

---

## State Machine

```
┌─────────────┐
│    IDLE     │  ← Default state
└──────┬──────┘
       │ Mouse down on file/folder + hold 150ms or move 5px
       ▼
┌─────────────┐
│  DRAGGING   │  ← Preview visible, tracking mouse
└──────┬──────┘
       │                        │
       │ Mouse over valid       │ Mouse released
       │ drop target            │ outside valid target
       ▼                        ▼
┌─────────────┐           ┌─────────────┐
│  HOVERING   │           │  CANCELLED  │ → Return to IDLE
└──────┬──────┘           └─────────────┘
       │
       │ Mouse released on valid target
       ▼
┌─────────────┐
│  DROPPING   │  ← Animation plays, move operation
└──────┬──────┘
       │
       │ Operation complete
       ▼
┌─────────────┐
│    IDLE     │
└─────────────┘
```

---

## Architecture

### New Files
```
src/ui/components/drag_drop.h     - Drag state, types, API
src/ui/components/drag_drop.c     - Implementation
```

### Data Structures

#### Multi-Selection Support (fs.h additions)
```c
/* Add to fs_state structure */
typedef struct {
    /* ... existing fields ... */
    
    /* Multi-selection support */
    u8 selected[FS_MAX_ENTRIES / 8];  /* Bitmask: 1 bit per entry */
    i32 selection_count;               /* Number of selected items */
    i32 selection_anchor;              /* Anchor for shift-click range */
} fs_state;

/* Selection API */
void FS_SelectSingle(fs_state *state, i32 index);           /* Clear others, select one */
void FS_SelectToggle(fs_state *state, i32 index);           /* Toggle without affecting others */
void FS_SelectRange(fs_state *state, i32 from, i32 to);     /* Range selection */
void FS_SelectAll(fs_state *state);                         /* Select all */
void FS_ClearSelection(fs_state *state);                    /* Deselect all */
b32  FS_IsSelected(fs_state *state, i32 index);             /* Check if selected */
i32  FS_GetSelectionCount(fs_state *state);                 /* Count selected */
i32  FS_GetFirstSelected(fs_state *state);                  /* Get first selected index */
i32  FS_GetNextSelected(fs_state *state, i32 after);        /* Iterator for selected items */
```

#### Drag/Drop State (drag_drop.h)
```c
#define DRAG_MAX_ITEMS 256  /* Max items in a single drag operation */

typedef enum {
    DRAG_STATE_IDLE,
    DRAG_STATE_PENDING,      /* Mouse down, waiting for threshold */
    DRAG_STATE_DRAGGING,     /* Actively dragging */
    DRAG_STATE_DROPPING,     /* Animation playing after drop */
} drag_state;

typedef enum {
    DROP_TARGET_NONE,
    DROP_TARGET_FOLDER,      /* Valid folder target */
    DROP_TARGET_PANEL,       /* Current directory of another panel */
    DROP_TARGET_INVALID,     /* Cannot drop here */
} drop_target_type;

/* Info for a single dragged item */
typedef struct {
    char path[FS_MAX_PATH];
    char name[FS_MAX_NAME];
    file_icon_type icon;
    b32 is_directory;
    u64 size;
} drag_item;

typedef struct {
    /* Drag state */
    drag_state state;
    
    /* Source items (multi-select support) */
    drag_item items[DRAG_MAX_ITEMS];
    i32 item_count;                   /* Number of items being dragged */
    u32 source_panel_idx;             /* Which panel the drag started from */
    
    /* Primary item (shown in preview, used for validation) */
    i32 primary_index;                /* Index in items[] of clicked item */
    
    /* Drag start position (for threshold detection) */
    v2i start_mouse_pos;
    u64 start_time_ms;
    
    /* Current mouse position */
    v2i current_mouse_pos;
    
    /* Drop target info */
    drop_target_type target_type;
    char target_path[FS_MAX_PATH];
    rect target_bounds;               /* For highlight rendering */
    u32 target_panel_idx;             /* Which panel the target is in */
    
    /* Animation state */
    smooth_value pickup_anim;         /* 0 -> 1 during pickup */
    smooth_value hover_glow;          /* Pulsing glow on valid target */
    smooth_value drop_anim;           /* For drop animation */
    
    /* Offset from cursor to preview center */
    v2i preview_offset;
} drag_drop_state;

/* Drag thresholds */
#define DRAG_THRESHOLD_DISTANCE 5    /* Pixels to move before drag starts */
#define DRAG_THRESHOLD_TIME_MS  150  /* Time to hold before drag starts */
```

### API

```c
/* Initialize drag/drop system */
void DragDrop_Init(drag_drop_state *state);

/* Start potential drag (on mouse down) */
void DragDrop_BeginPotential(drag_drop_state *state, 
                             const fs_entry *entry,
                             u32 panel_idx,
                             v2i mouse_pos);

/* Update drag state (call every frame) */
void DragDrop_Update(drag_drop_state *state, 
                     ui_context *ui,
                     layout_state *layout);

/* Check and set drop target (called by explorer during render) */
void DragDrop_CheckTarget(drag_drop_state *state,
                          const fs_entry *entry,
                          rect item_bounds,
                          u32 panel_idx);

/* Set panel as drop target (for dropping into panel's current dir) */
void DragDrop_CheckPanelTarget(drag_drop_state *state,
                               const char *panel_path,
                               rect panel_bounds,
                               u32 panel_idx);

/* Cancel current drag operation */
void DragDrop_Cancel(drag_drop_state *state);

/* Render drag preview (call last, on top of everything) */
void DragDrop_Render(drag_drop_state *state, ui_context *ui);

/* Render drop target highlight (call during list rendering) */
void DragDrop_RenderTargetHighlight(drag_drop_state *state, 
                                    ui_context *ui,
                                    rect target_bounds);

/* Check if dragging is active */
b32 DragDrop_IsActive(drag_drop_state *state);

/* Check if point is in current drop target */
b32 DragDrop_IsOverTarget(drag_drop_state *state, v2i point);

/* Get the source panel index */
u32 DragDrop_GetSourcePanel(drag_drop_state *state);
```

---

## Integration Points

### 1. Layout State (layout.h)
Add drag_drop_state to layout:

```c
typedef struct {
    /* ... existing fields ... */
    
    /* Drag and drop state (shared across panels) */
    drag_drop_state drag_drop;
} layout_state;
```

### 2. Explorer Update (explorer.c)
Modify mouse handling in `Explorer_Update`:

```c
/* In mouse down handler, after selection: */
if (input->mouse_pressed[MOUSE_LEFT] && !mouse_over_menu) {
    /* ... existing click handling ... */
    
    /* Start potential drag */
    fs_entry *entry = FS_GetSelectedEntry(&state->fs);
    if (entry && strcmp(entry->name, "..") != 0) {
        DragDrop_BeginPotential(layout->drag_drop, entry, panel_idx, mouse_pos);
    }
}
```

### 3. Explorer Render (explorer.c)
During item rendering, check for drop targets:

```c
/* For each visible folder item: */
if (DragDrop_IsActive(&layout->drag_drop) && entry->is_directory) {
    DragDrop_CheckTarget(&layout->drag_drop, entry, item_bounds, panel_idx);
    
    /* Render highlight if this is the target */
    if (/* is current target */) {
        DragDrop_RenderTargetHighlight(&layout->drag_drop, ui, item_bounds);
    }
}
```

### 4. Layout Render (layout.c)
Render preview on top of everything:

```c
void Layout_Render(...) {
    /* ... existing panel rendering ... */
    
    /* Render drag preview last (on top) */
    DragDrop_Render(&layout->drag_drop, ui);
}
```

### 5. File Operations
Execute the move when drop completes:

```c
/* In DragDrop_Update, when drop confirmed: */
if (state->state == DRAG_STATE_DROPPING && animation_complete) {
    /* Construct destination path */
    char dest_path[FS_MAX_PATH];
    FS_JoinPath(dest_path, FS_MAX_PATH, 
                state->target_path, 
                state->source_name);
    
    /* Perform move */
    if (FS_Rename(state->source_path, dest_path)) {
        /* Refresh both panels */
        Explorer_Refresh(&layout->panels[0].explorer);
        Explorer_Refresh(&layout->panels[1].explorer);
    }
    
    state->state = DRAG_STATE_IDLE;
}
```

---

## Visual Polish Details

### Colors (Theme Integration)
```c
/* Add to theme structure if not present */
color drag_preview_shadow;      /* rgba(0, 0, 0, 0.3) */
color drop_target_bg;           /* accent @ 15% opacity */
color drop_target_border;       /* accent @ 100% */
color drop_invalid_tint;        /* error @ 10% opacity */
```

### Animations
| Animation | Duration | Easing | Description |
|-----------|----------|--------|-------------|
| Pickup | 100ms | ease-out | Scale 1.0 → 0.9 |
| Hover pulse | 1000ms | sine | Opacity 0.15 → 0.25 |
| Drop | 200ms | ease-in | Preview flies to target |
| Highlight fade | 150ms | linear | Target highlight in/out |

### Cursor Changes
- **Grabbing**: When dragging (platform cursor or custom)
- **No-drop**: When over invalid target
- **Copy** (optional): Hold Ctrl to copy instead of move (future)

---

## Edge Cases to Handle

1. **Drag folder into itself** - Detect and prevent, show invalid
2. **Drag into descendant** - Detect and prevent (e.g., `/a/` into `/a/b/c/`)
3. **Same location drop** - No-op, don't show error
4. **Cross-filesystem move** - Falls back to copy+delete
5. **Permission denied** - Show error dialog
6. **File already exists** - Show confirmation dialog (future: overwrite/rename/skip)
7. **Long filenames** - Truncate in preview with ellipsis
8. **Drag during scroll** - Continue scrolling, update target
9. **Escape key** - Cancel drag at any point
10. **Window loses focus** - Cancel drag

---


## Future Enhancements

- **Drag to terminal** - Insert path into terminal
- **Drag from external** - Accept drops from OS file manager
- **Drag to external** - Drag to OS file manager
- **Touch support** - Long-press to drag on touch devices
- **Undo/Redo** - Undo last move operation
- **Progress indicator** - For large multi-file operations
