/*
 * context_menu.c - Right-Click Context Menu Implementation
 *
 * Context-aware popup menu with keyboard navigation and animations.
 * C99, handmade hero style.
 */

#include "context_menu.h"
#include "../../config/config.h"
#include "../../core/image.h"
#include "../../core/input.h"
#include "../../platform/platform.h"
#include "explorer.h"
#include <stdio.h>
#include <stdlib.h>
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
static void Action_CustomCommand(void *user_data);
static void PopulateCustomItems(context_menu_state *state);

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

typedef struct {
  char copy[64];
  char cut[64];
  char delete_item[64]; /* 'delete' is reserved keyword in C++ */
} multi_select_labels;

static void GetMultiSelectLabels(context_menu_state *state,
                                 multi_select_labels *labels) {
  i32 count = state->explorer ? FS_GetSelectionCount(&state->explorer->fs) : 1;

  if (count > 1) {
    snprintf(labels->copy, sizeof(labels->copy), "Copy (%d items)", count);
    snprintf(labels->cut, sizeof(labels->cut), "Cut (%d items)", count);
    snprintf(labels->delete_item, sizeof(labels->delete_item),
             "Delete (%d items)", count);
  } else {
    snprintf(labels->copy, sizeof(labels->copy), "Copy");
    snprintf(labels->cut, sizeof(labels->cut), "Cut");
    snprintf(labels->delete_item, sizeof(labels->delete_item), "Delete");
  }
}

static void PopulateFileMenu(context_menu_state *state) {
  state->item_count = 0;

  multi_select_labels labels;
  GetMultiSelectLabels(state, &labels);

  AddMenuItem(state, labels.copy, "Ctrl+C", Action_Copy, state, false);
  AddMenuItem(state, labels.cut, "Ctrl+X", Action_Cut, state, false);
  AddMenuItem(state, "Rename", "F2", Action_Rename, state, false);
  AddMenuItem(state, labels.delete_item, "Del", Action_Delete, state, true);
  AddMenuItem(state, "Copy Path", "", Action_CopyPath, state, true);

  i32 before_custom = state->item_count;
  PopulateCustomItems(state);

  /* Remove trailing separator if no custom items were added */
  if (state->item_count == before_custom && state->item_count > 0) {
    state->items[state->item_count - 1].separator_after = false;
  }
}

static void PopulateDirectoryMenu(context_menu_state *state) {
  state->item_count = 0;

  multi_select_labels labels;
  GetMultiSelectLabels(state, &labels);

  AddMenuItem(state, labels.copy, "Ctrl+C", Action_Copy, state, false);
  AddMenuItem(state, labels.cut, "Ctrl+X", Action_Cut, state, false);
  AddMenuItem(state, labels.delete_item, "Del", Action_Delete, state, true);
  AddMenuItem(state, "Copy Path", "", Action_CopyPath, state, true);

  i32 before_custom = state->item_count;
  PopulateCustomItems(state);

  /* Remove trailing separator if no custom items were added */
  if (state->item_count == before_custom && state->item_count > 0) {
    state->items[state->item_count - 1].separator_after = false;
  }
}

static void PopulateEmptyMenu(context_menu_state *state) {
  state->item_count = 0;
  AddMenuItem(state, "New File", "Ctrl+N", Action_NewFile, state, false);
  AddMenuItem(state, "New Directory", "Ctrl+Shift+N", Action_NewDir, state,
              true);
  AddMenuItem(state, "Paste", "Ctrl+V", Action_Paste, state, true);

  i32 before_custom = state->item_count;
  PopulateCustomItems(state);

  /* Remove trailing separator if no custom items were added */
  if (state->item_count == before_custom && state->item_count > 0) {
    state->items[state->item_count - 1].separator_after = false;
  }
}

#include "../../renderer/icons.h"

static file_icon_type GetIconTypeFromString(const char *name) {
  if (strcmp(name, "code") == 0)
    return WB_FILE_ICON_CODE_OTHER;
  if (strcmp(name, "terminal") == 0)
    return WB_FILE_ICON_EXECUTABLE;
  if (strcmp(name, "folder") == 0)
    return WB_FILE_ICON_DIRECTORY;
  if (strcmp(name, "file") == 0)
    return WB_FILE_ICON_FILE;
  if (strcmp(name, "image") == 0)
    return WB_FILE_ICON_IMAGE;
  if (strcmp(name, "audio") == 0)
    return WB_FILE_ICON_AUDIO;
  if (strcmp(name, "video") == 0)
    return WB_FILE_ICON_VIDEO;
  if (strcmp(name, "config") == 0)
    return WB_FILE_ICON_CONFIG;
  if (strcmp(name, "archive") == 0)
    return WB_FILE_ICON_ARCHIVE;
  return WB_FILE_ICON_UNKNOWN;
}

void ContextMenu_RefreshConfig(context_menu_state *state) {
  /* Free existing images before reloading */
  for (i32 j = 0; j < 8; j++) {
    if (state->custom_actions[j].icon_img) {
      Image_Free(state->custom_actions[j].icon_img);
      state->custom_actions[j].icon_img = NULL;
    }
  }
  state->custom_action_count = 0;

  /* Check indices 1 through 5 as requested */
  for (i32 i = 1; i <= 5; i++) {
    char key_icon[64], key_cmd[64], key_label[64];
    snprintf(key_icon, sizeof(key_icon), "context_menu.actions.%d.icon", i);
    snprintf(key_cmd, sizeof(key_cmd), "context_menu.actions.%d.cmd", i);
    snprintf(key_label, sizeof(key_label), "context_menu.actions.%d.label", i);

    const char *icon_name = Config_GetString(key_icon, "");
    const char *cmd = Config_GetString(key_cmd, "");

    if (icon_name[0] != '\0' && cmd[0] != '\0') {
      custom_action *action =
          &state->custom_actions[state->custom_action_count];

      /* Reset image */
      action->icon_img = NULL;

      /* Check if it's a file path (contains / or .) */
      if (strchr(icon_name, '/') || strchr(icon_name, '.') ||
          strchr(icon_name, '\\')) {
        action->icon_type = WB_FILE_ICON_IMAGE; /* Fallback type */
        action->icon_img = Image_Load(icon_name);
        if (!action->icon_img) {
          /* Failed to load, fallback to generic */
          action->icon_type = GetIconTypeFromString(icon_name);
        }
      } else {
        action->icon_type = GetIconTypeFromString(icon_name);
      }

      /* Fallback to generic code icon if unknown but specified (and no image
       * loaded) */
      if (action->icon_type == WB_FILE_ICON_UNKNOWN && !action->icon_img) {
        action->icon_type = WB_FILE_ICON_CODE_OTHER;
      }

      strncpy(action->command, cmd, sizeof(action->command) - 1);
      action->command[sizeof(action->command) - 1] = '\0';

      const char *label = Config_GetString(key_label, cmd);
      strncpy(action->label, label, sizeof(action->label) - 1);
      action->label[sizeof(action->label) - 1] = '\0';

      state->custom_action_count++;
    }
  }
}

static void PopulateCustomItems(context_menu_state *state) {
  i32 entry_count = Config_GetEntryCount();
  for (i32 i = 0; i < entry_count; i++) {
    const char *key = Config_GetEntryKey(i);
    if (key && strncmp(key, "context_menu.custom.", 20) == 0) {
      if (Config_GetEntryType(i) != WB_CONFIG_TYPE_STRING)
        continue;

      /* Skip the new icon actions if they accidentally get picked up */
      if (strncmp(key, "context_menu.actions.", 21) == 0)
        continue;

      const char *label = key + 20;
      const char *command = Config_GetString(key, "");

      if (command[0] != '\0' && state->custom_command_count < 8) {
        /* Store command template */
        strncpy(state->custom_commands[state->custom_command_count], command,
                sizeof(state->custom_commands[0]) - 1);
        state->custom_commands[state->custom_command_count]
                              [sizeof(state->custom_commands[0]) - 1] = '\0';

        /* Add to menu */
        AddMenuItem(state, label, "", Action_CustomCommand,
                    state->custom_commands[state->custom_command_count], false);
        state->custom_command_count++;
      }
    }
  }
}

static void ContextMenu_ExecuteSelectedItem(context_menu_state *state) {
  if (state->selected_index < 0 || state->selected_index >= state->item_count)
    return;

  menu_item *item = &state->items[state->selected_index];
  if (item->enabled && item->action) {
    item->action(item->user_data);
  }

  ContextMenu_Close(state);
}

/* ===== Public API ===== */

/* ===== Public API ===== */

void ContextMenu_Init(context_menu_state *state) {
  memset(state, 0, sizeof(*state));
  state->visible = false;
  state->selected_index = -1;
  state->selected_action_index = -1;
  state->action_row_height = 0;
  state->item_height = 28;
  state->menu_width = 180;

  /* Initialize animation */
  state->fade_anim.current = 0.0f;
  state->fade_anim.target = 0.0f;
  state->fade_anim.speed = 15.0f;

  /* Load initial config */
  ContextMenu_RefreshConfig(state);

  g_menu = state;
}

void ContextMenu_Show(context_menu_state *state, v2i position,
                      context_type type, const char *target_path,
                      explorer_state *explorer, ui_context *ui) {
  state->visible = true;
  state->position = position;
  state->type = type;
  state->selected_index = -1;
  state->selected_action_index = -1;
  state->explorer = explorer;
  state->ui = ui;

  if (target_path) {
    snprintf(state->target_path, sizeof(state->target_path), "%s", target_path);
  } else {
    state->target_path[0] = '\0';
  }

  state->custom_command_count = 0;

  /* Populate menu based on context type */
  switch (type) {
  case WB_CONTEXT_FILE:
    PopulateFileMenu(state);
    break;
  case WB_CONTEXT_DIRECTORY:
    PopulateDirectoryMenu(state);
    break;
  case WB_CONTEXT_EMPTY:
    PopulateEmptyMenu(state);
    break;
  default:
    state->item_count = 0;
    break;
  }

  /* Calculate action row height based on loaded actions */
  state->action_row_height = (state->custom_action_count > 0) ? 36 : 0;

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

  /* If we have many icons, ensure width is enough */
  if (state->custom_action_count > 0) {
    i32 icons_width = state->custom_action_count * 32 + 16;
    if (icons_width > state->menu_width)
      state->menu_width = icons_width;
  }

  /* Calculate initial menu height and set adjusted position
   * (will be refined in render when we know window dimensions) */
  i32 separator_count = 0;
  for (i32 i = 0; i < state->item_count; i++) {
    if (state->items[i].separator_after)
      separator_count++;
  }
  state->menu_height =
      state->item_count * state->item_height + separator_count * 8 + 8;
  state->menu_height += state->action_row_height;
  state->adjusted_position = position; /* Will be corrected in render */

  /* Start fade-in animation */
  state->fade_anim.target = 1.0f;

  /* Push focus to context menu */
  Input_PushFocus(WB_INPUT_TARGET_CONTEXT_MENU);
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

  /* Use pre-calculated adjusted position */
  rect menu_rect = {state->adjusted_position.x, state->adjusted_position.y,
                    state->menu_width, state->menu_height};

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
  if (input->key_pressed[WB_KEY_ESCAPE]) {
    ContextMenu_Close(state);
    return true;
  }

  /* Handle enter to execute */
  if (input->key_pressed[WB_KEY_RETURN] && state->selected_index >= 0) {
    ContextMenu_ExecuteSelectedItem(state);
    return true;
  }

  /* Handle up/down navigation */
  if (input->key_pressed[WB_KEY_UP]) {
    if (state->selected_index > 0) {
      state->selected_index--;
    } else if (state->selected_index == -1 && state->item_count > 0) {
      state->selected_index = state->item_count - 1;
    }
    return true;
  }

  if (input->key_pressed[WB_KEY_DOWN]) {
    if (state->selected_index < state->item_count - 1) {
      state->selected_index++;
    } else if (state->selected_index == -1 && state->item_count > 0) {
      state->selected_index = 0;
    }
    return true;
  }

  /* Handle mouse clicks - use adjusted position for hit testing */
  if (input->mouse_pressed[WB_MOUSE_LEFT] || input->mouse_pressed[WB_MOUSE_RIGHT]) {
    /* Use pre-calculated adjusted position for accurate hit testing */
    i32 menu_x = state->adjusted_position.x;
    i32 menu_y = state->adjusted_position.y;
    i32 menu_height = state->menu_height;

    rect menu_rect = {menu_x, menu_y, state->menu_width, menu_height};

    if (UI_PointInRect(input->mouse_pos, menu_rect)) {
      /* Click inside menu - check which item */
      if (input->mouse_pressed[WB_MOUSE_LEFT]) {
        i32 item_y = menu_y + 4;
        for (i32 i = 0; i < state->item_count; i++) {
          rect item_rect = {menu_x + 4, item_y, state->menu_width - 8,
                            state->item_height};
          if (UI_PointInRect(input->mouse_pos, item_rect)) {
            state->selected_index = i;
            ContextMenu_ExecuteSelectedItem(state);
            /* Consume the click */
            input->mouse_pressed[WB_MOUSE_LEFT] = false;
            return true;
          }
          item_y += state->item_height;
          if (state->items[i].separator_after) {
            item_y += 8;
          }
        }

        /* Check custom action row */
        if (state->custom_action_count > 0) {
          item_y += 5;
          i32 action_start_x = menu_x + 8;
          for (i32 i = 0; i < state->custom_action_count; i++) {
            rect action_rect = {action_start_x + i * 32, item_y, 28, 28};
            if (UI_PointInRect(input->mouse_pos, action_rect)) {
              Action_CustomCommand(state->custom_actions[i].command);
              ContextMenu_Close(state);
              input->mouse_pressed[WB_MOUSE_LEFT] = false;
              return true;
            }
          }
        }
      }
      /* Consume any click inside menu to prevent click-through */
      input->mouse_pressed[WB_MOUSE_LEFT] = false;
      input->mouse_pressed[WB_MOUSE_RIGHT] = false;
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

  /* ... inside ContextMenu_Render ... */
  /* Calculate menu dimensions */
  i32 separator_count = 0;
  for (i32 i = 0; i < state->item_count; i++) {
    if (state->items[i].separator_after)
      separator_count++;
  }

  i32 menu_height =
      state->item_count * state->item_height + separator_count * 8 + 8;

  /* Add action row height */
  menu_height += state->action_row_height;

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

  /* Store adjusted position for hit testing in Update */
  state->adjusted_position.x = menu_x;
  state->adjusted_position.y = menu_y;
  state->menu_height = menu_height;

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

    /* Ensure we don't select regular items if we are hovering action row */
    if (state->action_row_height > 0) {
      rect action_row_check = {menu_x,
                               menu_y + menu_height - state->action_row_height,
                               state->menu_width, state->action_row_height};
      if (UI_PointInRect(input->mouse_pos, action_row_check)) {
        hovered = false;
      }
    }

    /* Update selection on hover */
    if (hovered && state->visible) {
      state->selected_index = i;
      state->selected_action_index = -1; /* Deselect action row */
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

  /* ===== Custom Actions Row ===== */
  if (state->custom_action_count > 0) {
    /* Separator before actions */
    rect sep_rect = {menu_x + 8, item_y, state->menu_width - 16, 1};
    color sep_color = th->border;
    sep_color.a = (u8)(sep_color.a * fade);
    Render_DrawRect(renderer, sep_rect, sep_color);

    item_y += 5; // Spacing

    i32 action_start_x = menu_x + 8;

    for (i32 i = 0; i < state->custom_action_count; i++) {
      rect action_rect = {action_start_x + i * 32, item_y, 28, 28};

      b32 hovered = UI_PointInRect(input->mouse_pos, action_rect);
      b32 selected = (i == state->selected_action_index);

      if (hovered && state->visible) {
        state->selected_action_index = i;
        state->selected_index = -1; /* Deselect regular list */
        selected = true;
      }

      /* Highlight */
      if (selected) {
        color sel_bg = th->selection;
        sel_bg.a = (u8)(sel_bg.a * fade);
        Render_DrawRectRounded(renderer, action_rect, th->radius_sm, sel_bg);
      }

#include "../../core/image.h"

      /* Icon */
      custom_action *action = &state->custom_actions[i];
      color icon_col = Icon_GetTypeColor(action->icon_type, th);
      icon_col.a = (u8)(icon_col.a * fade);

      /* Using smaller rect for icon inside button */
      rect icon_draw_rect = {action_rect.x + 4, action_rect.y + 4, 20, 20};

      if (action->icon_img) {
        /* Draw user image */
        color tint = {255, 255, 255, (u8)(255 * fade)};
        Render_DrawImage(renderer, icon_draw_rect, action->icon_img, tint);
      } else {
        /* Draw vector icon */
        Icon_Draw(renderer, icon_draw_rect, action->icon_type, icon_col);
      }
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

static void Action_CustomCommand(void *user_data) {
  context_menu_state *state = g_menu;
  const char *command_template = (const char *)user_data;
  if (!state || !command_template)
    return;

  /* Calculate working directory (parent of target for files, target itself for
   * directories) */
  char working_dir[FS_MAX_PATH] = {0};
  if (state->target_path[0] != '\0') {
    strncpy(working_dir, state->target_path, sizeof(working_dir) - 1);
    if (!Platform_IsDirectory(working_dir)) {
      /* Strip filename to get directory */
      char *last_sep = strrchr(working_dir, '/');
#ifdef _WIN32
      char *win_sep = strrchr(working_dir, '\\');
      if (win_sep > last_sep)
        last_sep = win_sep;
#endif
      if (last_sep) {
        *last_sep = '\0';
      }
    }
  }

  /* Perform substitutions: %filepath -> target_path, %dir -> working_dir */
  char command[1024];
  char temp[1024];
  strncpy(temp, command_template, sizeof(temp) - 1);
  temp[sizeof(temp) - 1] = '\0';

  /* First pass: replace %filepath with quoted target path */
  command[0] = '\0';
  char *src = temp;
  char *dst = command;
  usize remaining = sizeof(command) - 1;

  while (*src && remaining > 0) {
    if (strncmp(src, "%filepath", 9) == 0) {
      /* Insert quoted path to handle spaces */
      usize path_len = strlen(state->target_path);
      if (path_len + 2 < remaining) {
        *dst++ = '"';
        memcpy(dst, state->target_path, path_len);
        dst += path_len;
        *dst++ = '"';
        remaining -= path_len + 2;
      }
      src += 9;
    } else if (strncmp(src, "%dir", 4) == 0) {
      /* Insert quoted directory path */
      usize dir_len = strlen(working_dir);
      if (dir_len + 2 < remaining) {
        *dst++ = '"';
        memcpy(dst, working_dir, dir_len);
        dst += dir_len;
        *dst++ = '"';
        remaining -= dir_len + 2;
      }
      src += 4;
    } else {
      *dst++ = *src++;
      remaining--;
    }
  }
  *dst = '\0';

  /* Pass show_window=true so GUI applications can display properly */
  Platform_SpawnProcess(command, working_dir[0] != '\0' ? working_dir : NULL,
                        true);
}
