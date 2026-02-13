/*
 * drag_drop.h - Drag and Drop System for File Explorer
 *
 * Handles multi-item drag operations with visual preview and
 * drop target highlighting in the file explorer.
 * C99, handmade hero style.
 */

#ifndef DRAG_DROP_H
#define DRAG_DROP_H

#include "../../core/animation.h"
#include "../../core/fs.h"
#include "../../core/types.h"

/* Forward declarations */
struct ui_context_s;
struct layout_state_s;

/* ===== Configuration ===== */

#define DRAG_MAX_ITEMS 256         /* Max items in a single drag operation */
#define DRAG_THRESHOLD_DISTANCE 5  /* Pixels to move before drag starts */
#define DRAG_THRESHOLD_TIME_MS 150 /* Time to hold before drag starts */
#define DRAG_PREVIEW_OFFSET_X 12   /* Pixels right of cursor */
#define DRAG_PREVIEW_OFFSET_Y 12   /* Pixels below cursor */
#define DRAG_PREVIEW_MAX_STACK 3   /* Max visible cards in stack */

/* ===== Drag State Enum ===== */

typedef enum {
  WB_DRAG_STATE_IDLE,     /* Not dragging */
  WB_DRAG_STATE_PENDING,  /* Mouse down, waiting for threshold */
  WB_DRAG_STATE_DRAGGING, /* Actively dragging, preview visible */
  WB_DRAG_STATE_DROPPING, /* Drop animation playing */
} drag_state_type;

/* ===== Drop Target Type ===== */

typedef enum {
  WB_DROP_TARGET_NONE,    /* No valid target */
  WB_DROP_TARGET_FOLDER,  /* Hovering over a folder */
  WB_DROP_TARGET_PANEL,   /* Dropping into panel's current directory */
  WB_DROP_TARGET_INVALID, /* Invalid target (e.g., folder into itself) */
} drop_target_type;

/* ===== Drag Item Info ===== */

typedef struct {
  char path[FS_MAX_PATH]; /* Full source path */
  char name[FS_MAX_NAME]; /* Display name */
  file_icon_type icon;    /* Icon type for preview */
  b32 is_directory;       /* Is this a directory? */
  u64 size;               /* File size (for info display) */
} drag_item;

/* ===== Drag Drop State ===== */

typedef struct drag_drop_state_s {
  /* Current drag state */
  drag_state_type state;

  /* Source items (multi-select support) */
  drag_item items[DRAG_MAX_ITEMS];
  i32 item_count;       /* Number of items being dragged */
  u32 source_panel_idx; /* Which panel the drag started from */

  /* Primary item (the one directly clicked, shown on top) */
  i32 primary_index; /* Index in items[] of clicked item */

  /* Drag initiation tracking */
  v2i start_mouse_pos; /* Mouse position at mouse down */
  u64 start_time_ms;   /* Timestamp of mouse down */

  /* Current mouse position (updated every frame) */
  v2i current_mouse_pos;

  /* Drop target info */
  drop_target_type target_type;
  char target_path[FS_MAX_PATH];
  rect target_bounds;   /* Bounds of current target for highlighting */
  u32 target_panel_idx; /* Which panel the target is in */

  /* Animation state */
  smooth_value pickup_anim; /* 0 -> 1 during pickup transition */
  smooth_value hover_glow;  /* Pulsing glow on valid target */
  smooth_value drop_anim;   /* 0 -> 1 during drop animation */

  /* Preview offset from cursor */
  v2i preview_offset;
} drag_drop_state;

/* ===== Lifecycle API ===== */

/* Initialize drag/drop system to default state */
void DragDrop_Init(drag_drop_state *state);

/* ===== Drag Initiation ===== */

/*
 * Begin a potential drag operation on mouse down.
 * Does not immediately start dragging - waits for threshold.
 *
 * @param state       Drag/drop state
 * @param fs_state    File system state (for getting selection)
 * @param clicked_idx Index of the entry that was clicked
 * @param panel_idx   Which panel this is from (0 or 1)
 * @param mouse_pos   Current mouse position
 * @param time_ms     Current time in milliseconds
 */
void DragDrop_BeginPotential(drag_drop_state *state, fs_state *fs,
                             i32 clicked_idx, u32 panel_idx, v2i mouse_pos,
                             u64 time_ms);

/* ===== Update ===== */

/*
 * Update drag state every frame.
 * Checks thresholds, updates mouse position, checks for drop, etc.
 *
 * @param state    Drag/drop state
 * @param ui       UI context for input
 * @param layout   Layout state for panel access
 * @param time_ms  Current time in milliseconds
 * @param dt       Delta time for animations
 */
void DragDrop_Update(drag_drop_state *state, struct ui_context_s *ui,
                     struct layout_state_s *layout, rect window_bounds,
                     u64 time_ms, f32 dt);

/* ===== Drop Target Detection ===== */

/*
 * Check if a folder entry is a valid drop target.
 * Call this during explorer render for each visible folder.
 *
 * @param state       Drag/drop state
 * @param entry       The folder entry to check
 * @param item_bounds Screen bounds of this entry
 * @param panel_idx   Which panel this entry is in
 */
void DragDrop_CheckTarget(drag_drop_state *state, const fs_entry *entry,
                          rect item_bounds, u32 panel_idx);

/*
 * Check if a panel's empty area is a valid drop target.
 * For dropping into the panel's current directory.
 *
 * @param state        Drag/drop state
 * @param panel_path   Current directory path of the panel
 * @param panel_bounds Screen bounds of the panel
 * @param panel_idx    Which panel this is
 */
void DragDrop_CheckPanelTarget(drag_drop_state *state, const char *panel_path,
                               rect panel_bounds, u32 panel_idx);

/* ===== Control ===== */

/* Cancel current drag operation (Escape key or focus loss) */
void DragDrop_Cancel(drag_drop_state *state);

/* ===== Rendering ===== */

/*
 * Render the drag preview (floating multi-item ghost).
 * Call this LAST in the render pass so it appears on top.
 *
 * @param state Drag/drop state
 * @param ui    UI context for rendering
 */
void DragDrop_RenderPreview(drag_drop_state *state, struct ui_context_s *ui);

/*
 * Render highlight for the current drop target.
 * Call this when rendering the target item/panel.
 *
 * @param state         Drag/drop state
 * @param ui            UI context for rendering
 * @param target_bounds Bounds to highlight
 */
void DragDrop_RenderTargetHighlight(drag_drop_state *state,
                                    struct ui_context_s *ui,
                                    rect target_bounds);

/* ===== Query Functions ===== */

/* Check if a drag operation is currently active */
b32 DragDrop_IsActive(drag_drop_state *state);

/* Check if currently in dragging state (preview visible) */
b32 DragDrop_IsDragging(drag_drop_state *state);

/* Get the source panel index */
u32 DragDrop_GetSourcePanel(drag_drop_state *state);

/* Check if the given bounds contain the current mouse position */
b32 DragDrop_IsMouseOver(drag_drop_state *state, rect bounds);

/* Check if dropping onto a path would be invalid (folder into itself/child) */
b32 DragDrop_IsInvalidTarget(drag_drop_state *state, const char *target_path);

#endif /* DRAG_DROP_H */
