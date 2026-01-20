/*
 * commands.c - Global Command Implementation
 */

#include "commands.h"
#include "config/config.h"
#include "core/input.h"
#include "core/text.h"
#include "platform/platform.h"
#include "ui/components/dialog.h"
#include "ui/components/explorer.h"
#include "ui/components/scroll_container.h"
#include "ui/layout.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Global context for callbacks */
static layout_state *g_layout = NULL;

void Commands_Init(layout_state *layout) { g_layout = layout; }

/* ===== Helper Macros ===== */

#define GET_ACTIVE_EXPLORER()                                                  \
  (g_layout ? &Layout_GetActivePanel(g_layout)->explorer : NULL)

/* ===== File Management ===== */

static void Cmd_FileRename(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_StartRename(e);
}

static void Cmd_FileDelete(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_ConfirmDelete(e, UI_GetContext());
}

static void Cmd_FileDuplicate(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_Duplicate(e);
}

static void Cmd_FileCopyPath(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    fs_entry *entry = Explorer_GetSelected(e);
    if (entry)
      Platform_SetClipboard(entry->path);
  }
}

static void Cmd_FileCopyRelativePath(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    fs_entry *entry = Explorer_GetSelected(e);
    if (entry)
      Platform_SetClipboard(entry->name); /* For now, just name */
  }
}

static void Cmd_FileCopyName(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    fs_entry *entry = Explorer_GetSelected(e);
    if (entry)
      Platform_SetClipboard(entry->name);
  }
}

static void Cmd_FileNewFile(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_StartCreateFile(e);
}

static void Cmd_FileNewFolder(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_StartCreateDir(e);
}

static void Cmd_FileRefresh(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_Refresh(e);
}

/* ===== Navigation ===== */

static void Cmd_NavParent(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    FS_NavigateUp(&e->fs);
}

static void Cmd_NavBack(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_GoBack(e);
}

static void Cmd_NavForward(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_GoForward(e);
}

static void Cmd_NavHome(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_NavigateTo(e, FS_GetHomePath(), false);
}

static void Cmd_NavRoot(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_NavigateTo(e, "/", false);
}

static void Cmd_NavFocusPath(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_FocusFilter(e);
}

/* ===== Terminal Integration ===== */

static void Cmd_TerminalToggle(void *u) {
  (void)u;
  if (g_layout)
    Layout_ToggleTerminal(g_layout);
}

static void Cmd_TerminalClear(void *u) {
  (void)u;
  if (g_layout) {
    panel *p = Layout_GetActivePanel(g_layout);
    if (p && p->terminal.terminal) {
      Terminal_Clear(p->terminal.terminal);
    }
  }
}

/* ===== Window & Layout ===== */

void Cmd_ViewToggleSplit(void *u) {
  (void)u;
  if (g_layout)
    Layout_ToggleMode(g_layout);
}

void Cmd_ViewFocusNextPane(void *u) {
  (void)u;
  if (g_layout) {
    u32 next = (g_layout->active_panel_idx + 1) % 2;
    Layout_SetActivePanel(g_layout, next);
  }
}

void Cmd_ViewToggleFullscreen(void *u) {
  (void)u;
  ui_context *ui = UI_GetContext();
  if (ui && ui->renderer && ui->renderer->window) {
    platform_window *win = (platform_window *)ui->renderer->window;
    Platform_SetFullscreen(win, !Platform_IsFullscreen(win));
  }
}

static void Cmd_WindowQuit(void *u) {
  (void)u;
  ui_context *ui = UI_GetContext();
  if (ui && ui->renderer && ui->renderer->window) {
    platform_window *win = (platform_window *)ui->renderer->window;
    Platform_RequestQuit(win);
  }
}

/* ===== System Integration ===== */

static void Cmd_SystemOpenDefault(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_OpenSelected(e);
}

/* ===== UI Extras ===== */

static void Cmd_ToggleHiddenFiles(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e)
    Explorer_ToggleHidden(e);
}

static void Cmd_ToggleAnimations(void *u) {
  (void)u;
  g_animations_enabled = !g_animations_enabled;
}

/* ===== Sorting ===== */

static void Cmd_SortByName(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    FS_SetSortOptions(&e->fs, SORT_BY_NAME, e->fs.sort_dir);
    Config_SetString("explorer.sort_type", "name");
    Config_Save();
  }
}

static void Cmd_SortBySize(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    FS_SetSortOptions(&e->fs, SORT_BY_SIZE, e->fs.sort_dir);
    Config_SetString("explorer.sort_type", "size");
    Config_Save();
  }
}

static void Cmd_SortByDate(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    FS_SetSortOptions(&e->fs, SORT_BY_DATE, e->fs.sort_dir);
    Config_SetString("explorer.sort_type", "date");
    Config_Save();
  }
}

static void Cmd_SortAscending(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    FS_SetSortOptions(&e->fs, e->fs.sort_by, SORT_ASCENDING);
    Config_SetString("explorer.sort_order", "ascending");
    Config_Save();
  }
}

static void Cmd_SortDescending(void *u) {
  (void)u;
  explorer_state *e = GET_ACTIVE_EXPLORER();
  if (e) {
    FS_SetSortOptions(&e->fs, e->fs.sort_by, SORT_DESCENDING);
    Config_SetString("explorer.sort_order", "descending");
    Config_Save();
  }
}

/* ===== Configuration ===== */

static void Cmd_ConfigReload(void *u) {
  (void)u;
  Config_Reload();
}

static void Cmd_ConfigDiagnostics(void *u) {
  (void)u;
  if (!g_layout)
    return;

  g_layout->show_config_diagnostics = true;
  ScrollContainer_Init(&g_layout->diagnostic_scroll);
  Input_PushFocus(INPUT_TARGET_DIALOG);
}

static void Cmd_ConfigOpenFile(void *u) {
  (void)u;
  const char *path = Config_GetPath();
  if (path) {
    Platform_OpenFile(path);
  }
}

/* ===== Registration ===== */

typedef struct {
  const char *name;
  const char *shortcut;
  const char *category;
  const char *tags;
  void (*callback)(void *);
} CommandDef;

static const CommandDef g_commands[] = {
    {"File: Copy Name", "palette", "File", "name", Cmd_FileCopyName},
    {"File: Copy Path", "palette", "File", "path location", Cmd_FileCopyPath},
    {"File: Copy Relative Path", "palette", "File", "path relative",
     Cmd_FileCopyRelativePath},
    {"File: Delete", "Delete", "File", "remove trash delete erase",
     Cmd_FileDelete},
    {"File: Duplicate", "palette", "File", "copy clone duplicate",
     Cmd_FileDuplicate},
    {"File: New File", "palette", "File", "add create new file",
     Cmd_FileNewFile},
    {"File: New Folder", "palette", "File", "add create directory mkdir folder",
     Cmd_FileNewFolder},
    {"File: Refresh", "palette", "File", "reload update refresh",
     Cmd_FileRefresh},
    {"File: Rename", "F2", "File", "move rename", Cmd_FileRename},
    {"File: Toggle Hidden Files", "Ctrl + .", "File", "dot hide visible hidden",
     Cmd_ToggleHiddenFiles},

    {"Nav: Focus Path", "palette", "Navigation", "filter search focus path",
     Cmd_NavFocusPath},
    {"Nav: Go Back", "Alt + Left", "Navigation", "history back", Cmd_NavBack},
    {"Nav: Go Forward", "Alt + Right", "Navigation", "history forward",
     Cmd_NavForward},
    {"Nav: Go Home", "palette", "Navigation", "user desk home", Cmd_NavHome},
    {"Nav: Go to Parent", "Alt + Up", "Navigation", "back up level parent",
     Cmd_NavParent},
    {"Nav: Go to Root", "palette", "Navigation", "slash base root",
     Cmd_NavRoot},

    {"System: Open Default", "Enter", "System", "execute run open",
     Cmd_SystemOpenDefault},

    {"Sort: Ascending", "palette", "Sort", "asc up order", Cmd_SortAscending},
    {"Sort: By Date", "palette", "Sort", "modified time sort", Cmd_SortByDate},
    {"Sort: By Name", "palette", "Sort", "alphabetical name sort",
     Cmd_SortByName},
    {"Sort: By Size", "palette", "Sort", "filesize bytes sort", Cmd_SortBySize},
    {"Sort: Descending", "palette", "Sort", "desc down order",
     Cmd_SortDescending},
    {"Terminal: Clear", "Ctrl + L", "Terminal", "reset console clear",
     Cmd_TerminalClear},
    {"Terminal: Toggle", "`", "Terminal", "show hide console terminal",
     Cmd_TerminalToggle},

    {"UI: Toggle Animations", "Ctrl + Alt + A", "UI",
     "motion graphics animation", Cmd_ToggleAnimations},

    {"View: Focus Next Pane", "Ctrl + Tab", "Layout", "switch panel tab pane",
     Cmd_ViewFocusNextPane},
    {"View: Toggle Fullscreen", "F11", "View", "maximize fullscreen",
     Cmd_ViewToggleFullscreen},
    {"View: Toggle Split", "Ctrl + \\", "Layout", "divide panel dual split",
     Cmd_ViewToggleSplit},

    {"Window: Quit", "Ctrl + Q", "Window", "exit close quit", Cmd_WindowQuit},
    {"Config: Reload", "palette", "Config",
     "settings preferences reload config", Cmd_ConfigReload},
    {"Config: Show Diagnostics", "palette", "Config",
     "settings health errors diagnostics", Cmd_ConfigDiagnostics},
    {"Config: Open File", "palette", "Config",
     "settings edit workbench.ini configuration", Cmd_ConfigOpenFile},
};

void Commands_Register(command_palette_state *palette) {
  for (usize i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
    const CommandDef *cmd = &g_commands[i];
    /* If shortcut is "palette", pass empty string to command palette */
    const char *shortcut =
        strcmp(cmd->shortcut, "palette") == 0 ? "" : cmd->shortcut;
    CommandPalette_RegisterCommand(palette, cmd->name, shortcut, cmd->category,
                                   cmd->tags, cmd->callback, NULL);
  }
}
