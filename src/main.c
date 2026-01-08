/*
 * main.c - Entry point for Workbench
 *
 * Initializes platform, creates window, runs event loop.
 * C99, handmade hero style.
 */

#include "animation.h"
#include "components/explorer.h"
#include "font.h"
#include "layout.h" /* Now using layout system */
#include "platform.h"
#include "renderer.h"
#include "theme.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Get framebuffer from platform (defined in platform_linux.c) */
extern void *Platform_GetFramebuffer(platform_window *window);

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("Workbench starting...\n");

  if (!Platform_Init()) {
    fprintf(stderr, "Failed to initialize platform\n");
    return 1;
  }

  if (!Font_SystemInit()) {
    fprintf(stderr, "Failed to initialize font system\n");
    Platform_Shutdown();
    return 1;
  }

  window_config config = {
      .title = "Workbench",
      .width = 1280,
      .height = 720,
      .resizable = true,
      .maximized = false,
  };

  platform_window *window = Platform_CreateWindow(&config);
  if (!window) {
    fprintf(stderr, "Failed to create window\n");
    Font_SystemShutdown();
    Platform_Shutdown();
    return 1;
  }

  printf("Window created (%dx%d)\n", config.width, config.height);

  /* Initialize renderer */
  renderer_backend *backend = Render_CreateSoftwareBackend();
  render_context renderer = {0};
  Render_Init(&renderer, backend);

  /* Load default font */
  font *main_font = Font_Load("sans", 16);
  font *mono_font = Font_Load("monospace", 14);
  renderer.default_font = main_font;

  if (!main_font) {
    fprintf(stderr, "Warning: Could not load system font\n");
  }

  /* Get theme */
  const theme *th = Theme_GetDefault();

  /* Initialize UI context */
  ui_context ui = {0};
  UI_Init(&ui, &renderer, th, main_font);

  /* Initialize memory arena for file system */
  void *arena_memory = malloc(Megabytes(16));
  memory_arena arena;
  ArenaInit(&arena, arena_memory, Megabytes(16));

  /* Initialize Layout (which inits explorers) */
  layout_state layout;
  Layout_Init(&layout, &arena);

  /* Navigate active panel to starting directory */
  if (argc > 1) {
    panel *p = Layout_GetActivePanel(&layout);
    if (p) {
      Explorer_NavigateTo(&p->explorer, argv[1]);
    }
  }

  u64 last_time = Platform_GetTimeMs();

  /* Main loop */
  i32 win_width = config.width;
  i32 win_height = config.height;

  while (!Platform_WindowShouldClose(window)) {
    platform_event event;

    /* Input state persists across frames, only reset one-shot events */
    static ui_input input = {0};

    /* Clear one-shot events from previous frame */
    memset(input.key_pressed, 0, sizeof(input.key_pressed));
    memset(input.key_released, 0, sizeof(input.key_released));
    memset(input.mouse_pressed, 0, sizeof(input.mouse_pressed));
    memset(input.mouse_released, 0, sizeof(input.mouse_released));
    input.scroll_delta = 0;
    input.text_input = 0;

    input.scroll_delta = 0;
    input.text_input = 0;

    /* Process all pending events */
    while (Platform_PollEvent(window, &event)) {
      switch (event.type) {
      case EVENT_QUIT:
        printf("Quit event received\n");
        break;

      case EVENT_KEY_DOWN:
        if (event.data.keyboard.key == KEY_ESCAPE) {
          panel *p = Layout_GetActivePanel(&layout);
          if (p && p->explorer.mode != EXPLORER_MODE_NORMAL) {
            Explorer_Cancel(&p->explorer);
          } else {
            printf("Escape pressed, exiting...\n");
            goto cleanup;
          }
        }
        /* Toggle Dual Panel Mode with F4 */
        if (event.data.keyboard.key == KEY_F4) {
          Layout_ToggleMode(&layout);
        }

        if (event.data.keyboard.key < KEY_COUNT) {
          if (!input.key_down[event.data.keyboard.key]) {
            input.key_pressed[event.data.keyboard.key] = true;
          }
          input.key_down[event.data.keyboard.key] = true;
        }
        input.modifiers = event.data.keyboard.modifiers;
        if (event.data.keyboard.character >= 32 &&
            event.data.keyboard.character < 128) {
          input.text_input = event.data.keyboard.character;
        }
        break;

      case EVENT_KEY_UP:
        if (event.data.keyboard.key < KEY_COUNT) {
          input.key_released[event.data.keyboard.key] = true;
          input.key_down[event.data.keyboard.key] = false;
        }
        input.modifiers = event.data.keyboard.modifiers;
        break;

      case EVENT_MOUSE_BUTTON_DOWN:
        if (event.data.mouse.button < MOUSE_BUTTON_COUNT) {
          if (!input.mouse_down[event.data.mouse.button]) {
            input.mouse_pressed[event.data.mouse.button] = true;
          }
          input.mouse_down[event.data.mouse.button] = true;
        }
        input.mouse_pos.x = event.data.mouse.x;
        input.mouse_pos.y = event.data.mouse.y;
        break;

      case EVENT_MOUSE_BUTTON_UP:
        if (event.data.mouse.button < MOUSE_BUTTON_COUNT) {
          input.mouse_released[event.data.mouse.button] = true;
          input.mouse_down[event.data.mouse.button] = false;
        }
        input.mouse_pos.x = event.data.mouse.x;
        input.mouse_pos.y = event.data.mouse.y;
        break;

      case EVENT_MOUSE_MOVE:
        input.mouse_pos.x = event.data.mouse.x;
        input.mouse_pos.y = event.data.mouse.y;
        break;

      case EVENT_MOUSE_SCROLL:
        input.scroll_delta += event.data.scroll.dy;
        break;

      case EVENT_WINDOW_RESIZE:
        win_width = event.data.resize.width;
        win_height = event.data.resize.height;
        break;

      default:
        break;
      }
    }

    /* Calculate delta time */
    u64 current_time = Platform_GetTimeMs();
    f32 dt_ms = (f32)(current_time - last_time);
    f32 dt = dt_ms / 1000.0f;
    last_time = current_time;

    /* Get framebuffer from platform */
    u32 *pixels = (u32 *)Platform_GetFramebuffer(window);
    Platform_GetWindowSize(window, &win_width, &win_height);

    if (pixels && win_width > 0 && win_height > 0) {
      Render_SetFramebuffer(&renderer, pixels, win_width, win_height);
      Render_BeginFrame(&renderer);

      /* Clear to background color */
      Render_Clear(&renderer, th->background);

      /* Begin UI frame */
      UI_BeginFrame(&ui, &input, dt);

      /* Calculate layout bounds early for update */
      i32 sidebar_width = 250;
      i32 layout_width = win_width - sidebar_width;
      if (layout_width < MIN_PANEL_WIDTH)
        layout_width = MIN_PANEL_WIDTH;
      rect layout_bounds = {0, 0, layout_width, win_height};

      /* Update layout logic (handles splitter interaction, animation) */
      Layout_Update(&layout, &ui, layout_bounds);

      /* ===== Layout System (Left/Main Area) ===== */
      /* Give layout e.g. 70% of width or flexible */
      /* Actually let's assume layout takes full left side for now, keeping
       * sidebar on right */

      /* Let's keep the existing structure: Explorer(s) on Left, Controls on
       * Right? Current code had Explorer Panel (width 350) and content area
       * (Rest). But the requirement is "Panel Layout", which usually implies
       * the explorers ARE the content. The buttons on the right were "Main
       * Content Area" in existing code. Let's make the Layout take the majority
       * of the space (Left), and keep the buttons on the Right as a "Sidebar".
       */
      Layout_Render(&layout, &ui, layout_bounds);

      /* ===== Separator Line ===== */
      Render_DrawRect(&renderer, (rect){layout_width, 0, 1, win_height},
                      th->border);

      /* ===== Sidebar (Right Side) ===== */
      UI_BeginLayout(
          UI_LAYOUT_VERTICAL,
          (rect){layout_width + 10, 10, sidebar_width - 20, win_height - 20});

      UI_Label("Workbench Controls");
      UI_Spacer(16);

      /* Control Buttons acting on ACTIVE panel */
      panel *active_panel = Layout_GetActivePanel(&layout);

      /* Buttons in vertical layout for sidebar */
      if (UI_Button("Home")) {
        if (active_panel)
          FS_NavigateHome(&active_panel->explorer.fs);
      }
      UI_Spacer(8);
      if (UI_Button("Refresh")) {
        if (active_panel)
          Explorer_Refresh(&active_panel->explorer);
      }
      UI_Spacer(8);
      if (UI_Button("New File")) {
        if (active_panel)
          Explorer_StartCreateFile(&active_panel->explorer);
      }
      UI_Spacer(8);
      if (UI_Button("New Folder")) {
        if (active_panel)
          Explorer_StartCreateDir(&active_panel->explorer);
      }
      UI_Spacer(8);
      if (UI_Button(layout.mode == LAYOUT_MODE_SINGLE
                        ? "C: Split View (F4)"
                        : "C: Single View (F4)")) {
        Layout_ToggleMode(&layout);
      }

      UI_Spacer(16);
      UI_Separator();
      UI_Spacer(16);

      /* Show current selection info from ACTIVE panel */
      fs_entry *selected =
          active_panel ? Explorer_GetSelected(&active_panel->explorer) : NULL;
      if (selected) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Selected: %s", selected->name);
        UI_Label(buf);

        UI_Spacer(8);

        if (selected->is_directory) {
          UI_PushStyleColor(UI_STYLE_TEXT_COLOR, th->text_muted);
          UI_Label("Directory");
          UI_PopStyle();
        } else {
          char size_str[32];
          FS_FormatSize(selected->size, size_str, sizeof(size_str));
          snprintf(buf, sizeof(buf), "Size: %s", size_str);
          UI_Label(buf);

          char time_str[64];
          FS_FormatTime(selected->modified_time, time_str, sizeof(time_str));
          snprintf(buf, sizeof(buf), "Modified: %s", time_str);
          UI_Label(buf);
        }
      } else {
        UI_PushStyleColor(UI_STYLE_TEXT_COLOR, th->text_muted);
        UI_Label("No file selected");
        UI_PopStyle();
      }

      UI_Spacer(32);

      /* Help text */
      UI_PushStyleColor(UI_STYLE_TEXT_COLOR, th->text_muted);
      UI_Label("Keyboard shortcuts:");
      UI_Spacer(4);
      UI_Label("  F4 - Toggle Split");
      UI_Label("  j/k - Navigate");
      UI_Label("  Enter - Open");
      UI_Label("  Backspace - Up");
      UI_Label("  Ctrl+H - Home");
      UI_Label("  F2 - Rename");
      UI_Label("  Del - Delete");
      UI_Label("  Tab - Switch Panel"); // Need to implement Tab switching
      UI_PopStyle();

      UI_EndLayout();

      /* End UI frame */
      UI_EndFrame(&ui);

      Render_EndFrame(&renderer);

      /* Commit the frame to display */
      Platform_PresentFrame(window);
    }

    /* Small sleep to avoid busy-looping when no events */
    Platform_SleepMs(16); /* ~60fps target */
  }

cleanup:
  UI_Shutdown(&ui);
  if (main_font)
    Font_Free(main_font);
  if (mono_font)
    Font_Free(mono_font);
  Render_Shutdown(&renderer);
  Platform_DestroyWindow(window);
  Font_SystemShutdown();
  Platform_Shutdown();
  free(arena_memory);

  printf("Workbench shutdown complete\n");
  return 0;
}
