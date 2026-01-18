/*
 * main.c - Entry point for Workbench
 *
 * Initializes platform, creates window, runs event loop.
 * C99, handmade hero style.
 */

#include "app_args.h"
#include "commands.h"
#include "config/config.h"
#include "core/args.h"
#include "core/assets_embedded.h"
#include "core/input.h"
#include "core/key_repeat.h"
#include "core/theme.h"
#include "platform/platform.h"
#include "renderer/font.h"
#include "renderer/renderer.h"
#include "ui/components/command_palette.h"
#include "ui/components/context_menu.h"
#include "ui/components/explorer.h"
#include "ui/layout.h" /* Now using layout system */
#include "ui/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {

  app_args args = Args_Parse(argc, argv);

  if (!Platform_Init()) {
    fprintf(stderr, "Failed to initialize platform\n");
    return 1;
  }

  Config_Init();

  window_config config = {
      .title = "Workbench",
      .width = (i32)Config_GetI64("window.width", 1280),
      .height = (i32)Config_GetI64("window.height", 720),
      .resizable = true,
      .maximized = Config_GetBool("window.maximized", false),
  };

  platform_window *window = Platform_CreateWindow(&config);
  if (!window) {
    fprintf(stderr, "Failed to create window\n");
    Config_Shutdown();
    Platform_Shutdown();
    return 1;
  }

  /* Initialize font system AFTER window creation (GDI needs display context) */
  if (!Font_SystemInit()) {
    fprintf(stderr, "Failed to initialize font system\n");
    Platform_DestroyWindow(window);
    Config_Shutdown();
    Platform_Shutdown();
    return 1;
  }

  /* Initialize renderer - try OpenGL first, fall back to software */
  renderer_backend *backend = NULL;
  const char *renderer_name = "Software";

  if (Render_OpenGLAvailable()) {
    backend = Render_CreateOpenGLBackend();
    if (backend) {
      renderer_name = "OpenGL";
    }
  }

  if (!backend) {
    backend = Render_CreateSoftwareBackend();
    renderer_name = "Software";
  }

  printf("Workbench starting (%s) ...\n", renderer_name);

  render_context renderer = {0};
  Render_Init(&renderer, backend);
  Render_SetWindow(&renderer, window);

  /* Update window title to indicate renderer */
  char title_with_renderer[256];
  snprintf(title_with_renderer, sizeof(title_with_renderer), "Workbench (%s)",
           renderer_name);
  Platform_SetWindowTitle(window, title_with_renderer);

  /* Load default font */
  i32 ui_font_size = (i32)Config_GetI64("ui.font_size", 16);
  font *main_font =
      Font_LoadFromFile("assets/fonts/JetBrainsMono-Regular.ttf", ui_font_size);
  if (!main_font) {
    main_font = Font_LoadFromMemory(asset_font_regular_data,
                                    asset_font_regular_size, ui_font_size);
  }

  i32 term_font_size = (i32)Config_GetI64("terminal.font_size", 14);
  font *mono_font = Font_LoadFromFile("assets/fonts/JetBrainsMono-Regular.ttf",
                                      term_font_size);
  if (!mono_font) {
    mono_font = Font_LoadFromMemory(asset_font_regular_data,
                                    asset_font_regular_size, term_font_size);
  }

  renderer.default_font = main_font;
  if (!main_font) {
    fprintf(stderr,
            "Fatal: Could not load ui font (neither file nor embedded)\n");
    Config_Shutdown();
    Platform_DestroyWindow(window);
    Font_SystemShutdown();
    Platform_Shutdown();
    return 1;
  }

  /* Get theme */
  Theme_InitFromConfig();
  const theme *th = Theme_GetDefault();

  /* Initialize UI context */
  ui_context ui = {0};
  UI_Init(&ui, &renderer, th, main_font, mono_font);
  ui.window_focused = true; /* Assume focused on start */

  /* Initialize memory arena for file system */
  void *arena_memory = malloc(Megabytes(64));
  memory_arena arena;
  ArenaInit(&arena, arena_memory, Megabytes(64));

  /* Initialize Layout (which inits explorers) */
  layout_state layout;
  Layout_Init(&layout, &arena);

  /* Navigate panels to starting directory/directories */
  Args_Handle(&layout, &args);

  /* Initialize Command Palette */
  command_palette_state palette;
  panel *initial_panel = Layout_GetActivePanel(&layout);
  CommandPalette_Init(&palette,
                      initial_panel ? &initial_panel->explorer.fs : NULL);

  /* Initialize Commands Module */
  Commands_Init(&layout);
  Commands_Register(&palette);

  /* Initialize Context Menu */
  context_menu_state context_menu;
  ContextMenu_Init(&context_menu);

  /* Store context menu reference in layout for explorer access */
  layout.context_menu = &context_menu;
  layout.panels[0].explorer.context_menu = &context_menu;
  layout.panels[1].explorer.context_menu = &context_menu;

  /* Initialize Input System */
  Input_Init();

  if (Config_HasErrors()) {
    layout.show_config_diagnostics = true;
    Input_PushFocus(INPUT_TARGET_DIALOG);
  }

  KeyRepeat_Init();

  u64 last_time = Platform_GetTimeMs();

  /* Main loop */
  i32 win_width = config.width;
  i32 win_height = config.height;
  i32 frame_count = 0;

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
            /* Only handle ESC at main level if explorer has focus and not in
             * dialog */
            input_target focus = Input_GetFocus();
            if (focus == INPUT_TARGET_EXPLORER) {
              panel *p = Layout_GetActivePanel(&layout);
              if (p && p->explorer.mode != EXPLORER_MODE_NORMAL) {
                Explorer_Cancel(&p->explorer);
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
            !(event.data.keyboard.modifiers &
              (MOD_CTRL | MOD_ALT | MOD_SHIFT))) {
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
            /* Store character for key repeat */
            if (event.data.keyboard.character >= 32 &&
                event.data.keyboard.character < 128) {
              KeyRepeat_SetCharacter(event.data.keyboard.key,
                                     event.data.keyboard.character);
            }
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
        input.modifiers = event.data.mouse.modifiers;
        break;

      case EVENT_MOUSE_BUTTON_UP:
        if (event.data.mouse.button < MOUSE_BUTTON_COUNT) {
          input.mouse_released[event.data.mouse.button] = true;
          input.mouse_down[event.data.mouse.button] = false;
        }
        input.mouse_pos.x = event.data.mouse.x;
        input.mouse_pos.y = event.data.mouse.y;
        input.modifiers = event.data.mouse.modifiers;
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

      case EVENT_WINDOW_FOCUS:
        ui.window_focused = true;
        break;

      case EVENT_WINDOW_UNFOCUS:
        ui.window_focused = false;
        break;

      default:
        break;
      }
    }

    /* Poll for configuration changes */
    if (Config_Poll()) {
      printf("Configuration reloaded due to file change\n");
      Theme_InitFromConfig();
      /* Update explorer settings */
      Layout_RefreshConfig(&layout);

      /* Reload fonts */
      i32 ui_font_size = (i32)Config_GetI64("ui.font_size", 16);
      i32 term_font_size = (i32)Config_GetI64("terminal.font_size", 14);

      font *new_main = Font_LoadFromFile(
          "assets/fonts/JetBrainsMono-Regular.ttf", ui_font_size);
      if (!new_main) {
        new_main = Font_LoadFromMemory(asset_font_regular_data,
                                       asset_font_regular_size, ui_font_size);
      }

      font *new_mono = Font_LoadFromFile(
          "assets/fonts/JetBrainsMono-Regular.ttf", term_font_size);
      if (!new_mono) {
        new_mono = Font_LoadFromMemory(asset_font_regular_data,
                                       asset_font_regular_size, term_font_size);
      }

      if (new_main && new_mono) {
        if (main_font)
          Font_Free(main_font);
        if (mono_font)
          Font_Free(mono_font);

        main_font = new_main;
        mono_font = new_mono;

        ui.font = main_font;
        ui.mono_font = mono_font;
        renderer.default_font = main_font;
      } else {
        /* If loading failed, cleanup whatever succeeded to avoid leaks/partial
         * state */
        if (new_main)
          Font_Free(new_main);
        if (new_mono)
          Font_Free(new_mono);
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

      /* Update key repeat system with current time */
      u64 frame_time = Platform_GetTimeMs();
      KeyRepeat_Update(input.key_down, input.key_pressed, frame_time);

      /* Add repeated text input if any */
      u32 repeated_text = KeyRepeat_GetTextInput();
      if (repeated_text > 0) {
        /* Update global input state for Input_GetTextInput() */
        Input_SetRepeatedTextInput(repeated_text);
        /* Update local input structs for UI components */
        input.text_input = repeated_text;
        ui.input.text_input = repeated_text;
      }

      /* Calculate layout bounds early for update */
      rect layout_bounds = {0, 0, win_width, win_height};

      /* Update layout logic (handles splitter interaction, animation) */
      /* Skip input processing for layout when command palette is open */
      if (!CommandPalette_IsOpen(&palette)) {
        Layout_Update(&layout, &ui, layout_bounds);
      }

      /* ===== Layout System (Full Window) ===== */
      Layout_Render(&layout, &ui, layout_bounds);

      /* ===== Context Menu (overlay) ===== */
      ContextMenu_Update(&context_menu, &ui);
      ContextMenu_Render(&context_menu, &ui, win_width, win_height);

      /* ===== Command Palette (overlay, rendered last) ===== */
      CommandPalette_Update(&palette, &ui);
      CommandPalette_Render(&palette, &ui, win_width, win_height);

      /* End input frame */
      Input_EndFrame();

      /* End UI frame */
      UI_EndFrame(&ui);

      Render_EndFrame(&renderer);

      /* Commit the frame to display if the backend doesn't handle it (e.g.
       * Software) */
      if (!renderer.backend->presents_frame) {
        Platform_PresentFrame(window);
      }
    }

    frame_count++;

    /* Small sleep to avoid busy-looping when no events */
    Platform_SleepMs(16); /* ~60fps target */
  }

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

  Config_Shutdown();
  printf("Workbench shutdown complete\n");
  return 0;
}
