/*
 * context_menu.c - Right-Click Context Menu Implementation
 *
 * Context-aware popup menu with keyboard navigation and animations.
 * C99, handmade hero style.
 */

#include "context_menu.h"
#include "explorer.h"
#include "input.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>

/* ===== Forward Declarations for Actions ===== */

static void Action_Copy(void *user_data);
static void Action_Cut(void *user_data);
static void Action_Paste(void *user_data);
static void Action_Rename(void *user_data);
static void Action_Delete(void *user_data);
static void Action_CopyPath(void *user_data);
static void Action_NewFile(void *user_data);
static void Action_NewDir(void *user_data);

/* Global reference to context menu for actions */
static context_menu_state *g_menu = NULL;

/* ===== Internal Helpers ===== */

static void AddMenuItem(context_menu_state *state, const char *label,
                        const char *shortcut, menu_action_fn action,
                        void *user_data, b32 separator_after) {
  if (state->item_count >= CONTEXT_MENU_MAX_ITEMS)
    return;

  menu_item *item = &state->items[state->item_count];
  snprintf(item->label, sizeof(item->label), "%s", label);
  snprintf(item->shortcut, sizeof(item->shortcut), "%s",
           shortcut ? shortcut : "");
  item->action = action;
  item->user_data = user_data;
  item->separator_after = separator_after;
  item->enabled = true;

  state->item_count++;
}

static void PopulateFileMenu(context_menu_state *state) {
  state->item_count = 0;
  AddMenuItem(state, "Copy", "Ctrl+C", Action_Copy, state, false);
  AddMenuItem(state, "Cut", "Ctrl+X", Action_Cut, state, false);
  AddMenuItem(state, "Rename", "F2", Action_Rename, state, false);
  AddMenuItem(state, "Delete", "Del", Action_Delete, state, true);
  AddMenuItem(state, "Copy Path", "", Action_CopyPath, state, false);
}

static void PopulateDirectoryMenu(context_menu_state *state) {
  state->item_count = 0;
  AddMenuItem(state, "New File", "Ctrl+N", Action_NewFile, state, false);
  AddMenuItem(state, "New Directory", "Ctrl+Shift+N", Action_NewDir, state,
              true);
  AddMenuItem(state, "Copy", "Ctrl+C", Action_Copy, state, false);
  AddMenuItem(state, "Cut", "Ctrl+X", Action_Cut, state, false);
  AddMenuItem(state, "Delete", "Del", Action_Delete, state, true);
  AddMenuItem(state, "Copy Path", "", Action_CopyPath, state, false);
}

static void PopulateEmptyMenu(context_menu_state *state) {
  state->item_count = 0;
  AddMenuItem(state, "New File", "Ctrl+N", Action_NewFile, state, false);
  AddMenuItem(state, "New Directory", "Ctrl+Shift+N", Action_NewDir, state,
              true);
  AddMenuItem(state, "Paste", "Ctrl+V", Action_Paste, state, false);
}

static void ExecuteSelectedItem(context_menu_state *state) {
  if (state->selected_index < 0 || state->selected_index >= state->item_count)
    return;

  menu_item *item = &state->items[state->selected_index];
  if (item->enabled && item->action) {
    item->action(item->user_data);
  }

  ContextMenu_Close(state);
}

/* ===== Public API ===== */

void ContextMenu_Init(context_menu_state *state) {
  memset(state, 0, sizeof(*state));
  state->visible = false;
  state->selected_index = -1;
  state->item_height = 28;
  state->menu_width = 180;

  /* Initialize animation */
  state->fade_anim.current = 0.0f;
  state->fade_anim.target = 0.0f;
  state->fade_anim.speed = 15.0f;

  g_menu = state;
}

void ContextMenu_Show(context_menu_state *state, v2i position,
                      context_type type, const char *target_path,
                      explorer_state *explorer, ui_context *ui) {
  state->visible = true;
  state->position = position;
  state->type = type;
  state->selected_index = -1;
  state->explorer = explorer;
  state->ui = ui;

  if (target_path) {
    snprintf(state->target_path, sizeof(state->target_path), "%s", target_path);
  } else {
    state->target_path[0] = '\0';
  }

  /* Populate menu based on context type */
  switch (type) {
  case CONTEXT_FILE:
    PopulateFileMenu(state);
    break;
  case CONTEXT_DIRECTORY:
    PopulateDirectoryMenu(state);
    break;
  case CONTEXT_EMPTY:
    PopulateEmptyMenu(state);
    break;
  default:
    state->item_count = 0;
    break;
  }

  /* Calculate optimal width */
  state->menu_width = 180; /* Minimum width */

  for (i32 i = 0; i < state->item_count; i++) {
    menu_item *item = &state->items[i];
    v2i label_size = UI_MeasureText(item->label, ui->font);
    v2i shortcut_size = {0, 0};

    if (item->shortcut[0] != '\0') {
      shortcut_size = UI_MeasureText(item->shortcut, ui->font);
    }

    /* Padding (8) + Label + Spacing (32) + Shortcut + Padding (8) */
    i32 needed_width = 8 + label_size.x + 32 + shortcut_size.x + 8;
    if (needed_width > state->menu_width) {
      state->menu_width = needed_width;
    }
  }

  /* Start fade-in animation */
  state->fade_anim.target = 1.0f;

  /* Push focus to context menu */
  Input_PushFocus(INPUT_TARGET_CONTEXT_MENU);
}

void ContextMenu_Close(context_menu_state *state) {
  if (!state->visible)
    return;

  /* Don't set visible=false immediately - let fade-out animate */
  state->fade_anim.target = 0.0f;
  Input_PopFocus();
  /* Mark as closing but still animating */
  state->visible = false;
}

b32 ContextMenu_IsVisible(context_menu_state *state) {
  return state->visible || state->fade_anim.current > 0.01f;
}

b32 ContextMenu_IsMouseOver(context_menu_state *state, v2i mouse_pos) {
  if (!state->visible)
    return false;

  i32 separator_count = 0;
  for (i32 i = 0; i < state->item_count; i++) {
    if (state->items[i].separator_after)
      separator_count++;
  }

  i32 menu_height =
      state->item_count * state->item_height + separator_count * 8 + 8;

  rect menu_rect = {state->position.x, state->position.y, state->menu_width,
                    menu_height};

  return UI_PointInRect(mouse_pos, menu_rect);
}

b32 ContextMenu_Update(context_menu_state *state, ui_context *ui) {
  /* Always update fade animation */
  SmoothValue_Update(&state->fade_anim, ui->dt);

  /* Menu is fully closed when animation is done */
  if (!state->visible && state->fade_anim.current < 0.01f)
    return false;

  /* Still animating closed - don't process input, just render fade-out */
  if (!state->visible)
    return false;

  ui_input *input = &ui->input;

  /* Handle escape to close */
  if (input->key_pressed[KEY_ESCAPE]) {
    ContextMenu_Close(state);
    return true;
  }

  /* Handle enter to execute */
  if (input->key_pressed[KEY_RETURN] && state->selected_index >= 0) {
    ExecuteSelectedItem(state);
    return true;
  }

  /* Handle up/down navigation */
  if (input->key_pressed[KEY_UP]) {
    if (state->selected_index > 0) {
      state->selected_index--;
    } else if (state->selected_index == -1 && state->item_count > 0) {
      state->selected_index = state->item_count - 1;
    }
    return true;
  }

  if (input->key_pressed[KEY_DOWN]) {
    if (state->selected_index < state->item_count - 1) {
      state->selected_index++;
    } else if (state->selected_index == -1 && state->item_count > 0) {
      state->selected_index = 0;
    }
    return true;
  }

  /* Handle mouse clicks - calculate menu rect for hit testing */
  if (input->mouse_pressed[MOUSE_LEFT] || input->mouse_pressed[MOUSE_RIGHT]) {
    /* Need to calculate menu dimensions for hit testing */
    i32 separator_count = 0;
    for (i32 i = 0; i < state->item_count; i++) {
      if (state->items[i].separator_after)
        separator_count++;
    }
    i32 menu_height =
        state->item_count * state->item_height + separator_count * 8 + 8;

    rect menu_rect = {state->position.x, state->position.y, state->menu_width,
                      menu_height};

    if (UI_PointInRect(input->mouse_pos, menu_rect)) {
      /* Click inside menu - check which item */
      if (input->mouse_pressed[MOUSE_LEFT]) {
        i32 item_y = state->position.y + 4;
        for (i32 i = 0; i < state->item_count; i++) {
          rect item_rect = {state->position.x + 4, item_y,
                            state->menu_width - 8, state->item_height};
          if (UI_PointInRect(input->mouse_pos, item_rect)) {
            state->selected_index = i;
            ExecuteSelectedItem(state);
            /* Consume the click */
            input->mouse_pressed[MOUSE_LEFT] = false;
            return true;
          }
          item_y += state->item_height;
          if (state->items[i].separator_after) {
            item_y += 8;
          }
        }
      }
      /* Consume any click inside menu to prevent click-through */
      input->mouse_pressed[MOUSE_LEFT] = false;
      input->mouse_pressed[MOUSE_RIGHT] = false;
    } else {
      /* Click outside menu - close it */
      ContextMenu_Close(state);
      /* Don't consume the click - let it go through to elements behind */
    }
    return true;
  }

  return true; /* Consume all input when menu is open */
}

void ContextMenu_Render(context_menu_state *state, ui_context *ui,
                        i32 win_width, i32 win_height) {
  if (!ContextMenu_IsVisible(state))
    return;

  render_context *renderer = ui->renderer;
  const theme *th = ui->theme;
  font *f = ui->font;
  ui_input *input = &ui->input;

  f32 fade = state->fade_anim.current;

  /* Calculate menu dimensions */
  i32 separator_count = 0;
  for (i32 i = 0; i < state->item_count; i++) {
    if (state->items[i].separator_after)
      separator_count++;
  }

  i32 menu_height =
      state->item_count * state->item_height + separator_count * 8 + 8;

  /* Adjust position if menu would go off-screen */
  i32 menu_x = state->position.x;
  i32 menu_y = state->position.y;

  if (menu_x + state->menu_width > win_width) {
    menu_x = win_width - state->menu_width - 4;
  }
  if (menu_y + menu_height > win_height) {
    menu_y = win_height - menu_height - 4;
  }
  if (menu_x < 4)
    menu_x = 4;
  if (menu_y < 4)
    menu_y = 4;

  rect menu_rect = {menu_x, menu_y, state->menu_width, menu_height};

  /* ===== Shadow ===== */
  color shadow = {0, 0, 0, (u8)(60 * fade)};
  rect shadow_rect = {menu_x + 4, menu_y + 4, state->menu_width, menu_height};
  Render_DrawRectRounded(renderer, shadow_rect, th->radius_md, shadow);

  /* ===== Background ===== */
  color bg = th->panel;
  bg.a = (u8)(bg.a * fade);
  Render_DrawRectRounded(renderer, menu_rect, th->radius_md, bg);

  /* ===== Border ===== */
  color border = th->border;
  border.a = (u8)(border.a * fade);
  rect border_rect = {menu_x - 1, menu_y - 1, state->menu_width + 2,
                      menu_height + 2};
  Render_DrawRectRounded(renderer, border_rect, th->radius_md + 1, border);
  Render_DrawRectRounded(renderer, menu_rect, th->radius_md, bg);

  /* ===== Items ===== */
  i32 item_y = menu_y + 4;

  for (i32 i = 0; i < state->item_count; i++) {
    menu_item *item = &state->items[i];

    rect item_rect = {menu_x + 4, item_y, state->menu_width - 8,
                      state->item_height};

    /* Check for hover - only update selection on hover if menu is still open */
    b32 hovered = UI_PointInRect(input->mouse_pos, item_rect);
    b32 selected = (i == state->selected_index);

    /* Update selection on hover */
    if (hovered && state->visible) {
      state->selected_index = i;
      selected = true;
    }

    /* Highlight */
    if (selected) {
      color sel_bg = th->selection;
      sel_bg.a = (u8)(sel_bg.a * fade);
      Render_DrawRectRounded(renderer, item_rect, th->radius_sm, sel_bg);
    }

    /* Label */
    color text_color = item->enabled ? th->text : th->text_disabled;
    text_color.a = (u8)(text_color.a * fade);

    i32 text_x = item_rect.x + 8;
    i32 text_y = item_rect.y + (state->item_height - 16) / 2;
    Render_DrawText(renderer, (v2i){text_x, text_y}, item->label, f,
                    text_color);

    /* Shortcut */
    if (item->shortcut[0] != '\0') {
      color shortcut_color = th->text_muted;
      shortcut_color.a = (u8)(shortcut_color.a * fade);
      v2i shortcut_size = UI_MeasureText(item->shortcut, f);
      i32 shortcut_x = item_rect.x + item_rect.w - shortcut_size.x - 8;
      Render_DrawText(renderer, (v2i){shortcut_x, text_y}, item->shortcut, f,
                      shortcut_color);
    }

    item_y += state->item_height;

    /* Separator */
    if (item->separator_after) {
      item_y += 4;
      rect sep_rect = {menu_x + 8, item_y, state->menu_width - 16, 1};
      color sep_color = th->border;
      sep_color.a = (u8)(sep_color.a * fade);
      Render_DrawRect(renderer, sep_rect, sep_color);
      item_y += 4;
    }
  }
}

/* ===== Action Implementations ===== */

static void Action_Copy(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->explorer) {
    Explorer_Copy(state->explorer);
  }
}

static void Action_Cut(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->explorer) {
    Explorer_Cut(state->explorer);
  }
}

static void Action_Paste(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->explorer) {
    Explorer_Paste(state->explorer);
  }
}

static void Action_Rename(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->explorer) {
    Explorer_StartRename(state->explorer);
  }
}

static void Action_Delete(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->explorer && state->ui) {
    Explorer_ConfirmDelete(state->explorer, state->ui);
  }
}

static void Action_CopyPath(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->target_path[0] != '\0') {
    Platform_SetClipboard(state->target_path);
  }
}

static void Action_NewFile(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->explorer) {
    Explorer_StartCreateFile(state->explorer);
  }
}

static void Action_NewDir(void *user_data) {
  context_menu_state *state = (context_menu_state *)user_data;
  if (state->explorer) {
    Explorer_StartCreateDir(state->explorer);
  }
}
