/*
 * main.c - Entry point for Workbench
 *
 * Initializes platform, creates window, runs event loop.
 * C99, handmade hero style.
 */

#include "animation.h"
#include "commands.h"
#include "components/command_palette.h"
#include "components/explorer.h"
#include "font.h"
#include "input.h"
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
  font *main_font = Font_LoadFromFile("assets/fonts/JetBrainsMono-Regular.ttf", 16);
  font *mono_font = Font_LoadFromFile("assets/fonts/JetBrainsMono-Regular.ttf", 14);
  renderer.default_font = main_font;

  if (!main_font) {
    fprintf(stderr, "Warning: Could not load ui font assets/fonts/JetBrainsMono-Regular.ttf\n");
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

  /* Initialize Command Palette */
  command_palette_state palette;
  panel *initial_panel = Layout_GetActivePanel(&layout);
  CommandPalette_Init(&palette, initial_panel ? &initial_panel->explorer.fs : NULL);

  /* Initialize Commands Module */
  Commands_Init(&layout);
  Commands_Register(&palette);

  /* Initialize Input System */
  Input_Init();

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

      case EVENT_KEY_DOWN: {
        bool consumed = false;

        /* Command palette keybindings */
        if (event.data.keyboard.key == KEY_P && 
            (event.data.keyboard.modifiers & MOD_CTRL)) {
          if (event.data.keyboard.modifiers & MOD_SHIFT) {
            CommandPalette_Open(&palette, PALETTE_MODE_COMMAND);
          } else {
            CommandPalette_Open(&palette, PALETTE_MODE_FILE);
          }
          consumed = true;
        }
        
        if (event.data.keyboard.key == KEY_ESCAPE) {
          /* Let CommandPalette_Update handle ESC if palette is open */
          if (!CommandPalette_IsOpen(&palette)) {
            /* Only handle ESC at main level if explorer has focus and not in dialog */
            input_target focus = Input_GetFocus();
            if (focus == INPUT_TARGET_EXPLORER) {
              panel *p = Layout_GetActivePanel(&layout);
              if (p && p->explorer.mode != EXPLORER_MODE_NORMAL) {
                Explorer_Cancel(&p->explorer);
              } else {
                printf("Escape pressed, exiting...\n");
                goto cleanup;
              }
            }
            /* Terminal and Dialog handle their own ESC */
          }
          consumed = true;
        }
        /* Toggle Dual Panel Mode with Ctrl + / */
        if (event.data.keyboard.key == KEY_SLASH && 
           (event.data.keyboard.modifiers & MOD_CTRL)) {
          Layout_ToggleMode(&layout);
          consumed = true;
        }

        /* Toggle Terminal with ` (backtick) */
        if (event.data.keyboard.key == KEY_GRAVE &&
            !(event.data.keyboard.modifiers & (MOD_CTRL | MOD_ALT | MOD_SHIFT))) {
          Layout_ToggleTerminal(&layout);
          consumed = true;
        }

        /* Focus Split 1 (Alt + 1) */
        if (event.data.keyboard.key == KEY_1 &&
            (event.data.keyboard.modifiers & MOD_ALT)) {
          Layout_SetActivePanel(&layout, 0);
          consumed = true;
        }

        /* Focus Split 2 (Alt + 2) */
        if (event.data.keyboard.key == KEY_2 &&
            (event.data.keyboard.modifiers & MOD_ALT)) {
          Layout_SetActivePanel(&layout, 1);
          consumed = true;
        }

        if (event.data.keyboard.key < KEY_COUNT) {
          if (!input.key_down[event.data.keyboard.key]) {
            input.key_pressed[event.data.keyboard.key] = true;
          }
          input.key_down[event.data.keyboard.key] = true;
        }
        input.modifiers = event.data.keyboard.modifiers;
        if (!consumed && event.data.keyboard.character >= 32 &&
            event.data.keyboard.character < 128) {
          input.text_input = event.data.keyboard.character;
        }
        break;
      }

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

      /* Begin input frame with collected input */
      Input_BeginFrame(&input);

      /* Calculate layout bounds early for update */
      rect layout_bounds = {0, 0, win_width, win_height};

      /* Update layout logic (handles splitter interaction, animation) */
      /* Skip input processing for layout when command palette is open */
      if (!CommandPalette_IsOpen(&palette)) {
        Layout_Update(&layout, &ui, layout_bounds);
      }

      /* ===== Layout System (Full Window) ===== */
      Layout_Render(&layout, &ui, layout_bounds);

      /* ===== Command Palette (overlay, rendered last) ===== */
      CommandPalette_Update(&palette, &ui);
      CommandPalette_Render(&palette, &ui, win_width, win_height);

      /* End input frame */
      Input_EndFrame();

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
