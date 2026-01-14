/*
 * commands.c - Global Command Implementation
 */

#include "commands.h"
#include "ui/components/explorer.h"
#include <stddef.h>

/* Global context for callbacks */
static layout_state *g_layout = NULL;

void Commands_Init(layout_state *layout) { g_layout = layout; }

/* ===== Command Implementations ===== */

static void Cmd_ToggleSplitView(void *user_data) {
  (void)user_data;
  if (g_layout) {
    Layout_ToggleMode(g_layout);
  }
}

static void Cmd_Home(void *user_data) {
  (void)user_data;
  if (g_layout) {
    panel *p = Layout_GetActivePanel(g_layout);
    if (p) {
      FS_NavigateHome(&p->explorer.fs);
    }
  }
}

static void Cmd_Refresh(void *user_data) {
  (void)user_data;
  if (g_layout) {
    panel *p = Layout_GetActivePanel(g_layout);
    if (p) {
      Explorer_Refresh(&p->explorer);
    }
  }
}

static void Cmd_NewFile(void *user_data) {
  (void)user_data;
  if (g_layout) {
    panel *p = Layout_GetActivePanel(g_layout);
    if (p) {
      Explorer_StartCreateFile(&p->explorer);
    }
  }
}

static void Cmd_NewFolder(void *user_data) {
  (void)user_data;
  if (g_layout) {
    panel *p = Layout_GetActivePanel(g_layout);
    if (p) {
      Explorer_StartCreateDir(&p->explorer);
    }
  }
}

static void Cmd_ToggleHiddenFiles(void *user_data) {
  (void)user_data;
  if (g_layout) {
    panel *p = Layout_GetActivePanel(g_layout);
    if (p) {
      p->explorer.show_hidden = !p->explorer.show_hidden;
      Explorer_Refresh(&p->explorer);
    }
  }
}

static void Cmd_ToggleAnimations(void *user_data) {
  (void)user_data;
  g_animations_enabled = !g_animations_enabled;
}

/* ===== Registration ===== */

void Commands_Register(command_palette_state *palette) {
  CommandPalette_RegisterCommand(palette, "Toggle Split View", "F4", "Layout",
                                 Cmd_ToggleSplitView, NULL);
  CommandPalette_RegisterCommand(palette, "Home", "Ctrl + H", "Navigation",
                                 Cmd_Home, NULL);
  CommandPalette_RegisterCommand(palette, "Refresh", "Ctrl + R", "File",
                                 Cmd_Refresh, NULL);
  CommandPalette_RegisterCommand(palette, "New File", "Ctrl + N", "File",
                                 Cmd_NewFile, NULL);
  CommandPalette_RegisterCommand(palette, "New Folder", "Ctrl + Shift + N",
                                 "File", Cmd_NewFolder, NULL);
  CommandPalette_RegisterCommand(palette, "Toggle Hidden Files", "Ctrl + .",
                                 "File", Cmd_ToggleHiddenFiles, NULL);
  CommandPalette_RegisterCommand(palette, "Toggle Animations", "Ctrl + Alt + A",
                                 "UI", Cmd_ToggleAnimations, NULL);
}
