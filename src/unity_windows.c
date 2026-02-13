/*
 * unity_windows.c - Single translation unit build for Windows
 *
 * This file #includes all source files for the Windows build.
 */

/* === Core === */
#include "core/animation.c"
#include "core/args.c"
#include "core/assets_embedded.c"
#include "core/fs.c"
#include "core/image.c"
#include "core/fuzzy_match.c"
#include "core/input.c"
#include "core/key_repeat.c"
#include "core/text.c"
#include "core/theme.c"

/* === Platform (Windows) === */
#include "platform/windows/windows_clipboard.c"
#include "platform/windows/windows_dragdrop.c"
#include "platform/windows/windows_events.c"
#include "platform/windows/windows_filesystem.c"
#include "platform/windows/windows_font.c"
#include "platform/windows/windows_fs_watcher.c"
#include "platform/windows/windows_platform.c"
#include "platform/windows/windows_process.c"
#include "platform/windows/windows_pty.c"
#include "platform/windows/windows_time.c"
#include "platform/windows/windows_window.c"

/* === Renderer === */
#include "renderer/icons.c"
#include "renderer/renderer_opengl.c"
#include "renderer/renderer_software.c"

/* === UI === */
#include "ui/components/breadcrumb.c"
#include "ui/components/command_palette.c"
#include "ui/components/config_diagnostics.c"
#include "ui/components/context_menu.c"
#include "ui/components/dialog.c"
#include "ui/components/drag_drop.c"
#include "ui/components/explorer.c"
#include "ui/components/file_item.c"
#include "ui/components/quick_filter.c"
#include "ui/components/scroll_container.c"
#include "ui/components/terminal_panel.c"
#include "ui/components/text_input.c"
#include "ui/components/widgets.c"
#include "ui/layout.c"
#include "ui/ui.c"

/* === Terminal === */
#include "terminal/ansi_parser.c"
#include "terminal/command_history.c"
#include "terminal/suggestion.c"
#include "terminal/terminal.c"

/* === Config === */
#include "config/config.c"
#include "config/config_parser.c"

/* === App === */
#include "app_args.c"
#include "commands.c"

/* === Entry Point (must be last) === */
#include "main.c"
