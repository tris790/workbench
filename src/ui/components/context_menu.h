/*
 * context_menu.h - Right-Click Context Menu for Workbench
 *
 * Context-aware popup menu for file operations.
 * C99, handmade hero style.
 */

#ifndef CONTEXT_MENU_H
#define CONTEXT_MENU_H

#include "../../core/animation.h"
#include "../ui.h"

/* ===== Configuration ===== */

#define CONTEXT_MENU_MAX_ITEMS 16

/* Forward declaration for explorer */
struct explorer_state_s;

/* ===== Types ===== */

typedef enum {
  CONTEXT_NONE = 0,
  CONTEXT_FILE,      /* Right-click on file */
  CONTEXT_DIRECTORY, /* Right-click on folder */
  CONTEXT_EMPTY,     /* Right-click on empty space */
} context_type;

/* Command callback for menu items */
typedef void (*menu_action_fn)(void *user_data);

/* Single menu item */
typedef struct {
  char label[64];
  char shortcut[32];
  menu_action_fn action;
  void *user_data;
  b32 separator_after;
  b32 enabled;
} menu_item;

/* Context menu state */
typedef struct context_menu_state_s {
  /* Items */
  menu_item items[CONTEXT_MENU_MAX_ITEMS];
  i32 item_count;

  /* Position and selection */
  v2i position;       /* Screen position (top-left) */
  i32 selected_index; /* Keyboard selection (-1 = none) */

  /* Context */
  context_type type;
  char target_path[FS_MAX_PATH]; /* Path of target file/dir */

  /* Visibility and animation */
  b32 visible;
  smooth_value fade_anim;

  /* Cached dimensions */
  i32 item_height;
  i32 menu_width;

  /* References for action callbacks (set when menu is shown) */
  struct explorer_state_s *explorer;
  ui_context *ui;
} context_menu_state;

/* ===== Context Menu API ===== */

/* Initialize context menu state */
void ContextMenu_Init(context_menu_state *state);

/* Show context menu at position with given context */
void ContextMenu_Show(context_menu_state *state, v2i position,
                      context_type type, const char *target_path,
                      struct explorer_state_s *explorer, ui_context *ui);

/* Close the context menu */
void ContextMenu_Close(context_menu_state *state);

/* Check if menu is visible */
b32 ContextMenu_IsVisible(context_menu_state *state);

/* Check if mouse is hovering over the menu */
b32 ContextMenu_IsMouseOver(context_menu_state *state, v2i mouse_pos);

/* Update context menu (handles input, returns true if consuming input) */
b32 ContextMenu_Update(context_menu_state *state, ui_context *ui);

/* Render context menu overlay */
void ContextMenu_Render(context_menu_state *state, ui_context *ui,
                        i32 win_width, i32 win_height);

#endif /* CONTEXT_MENU_H */
