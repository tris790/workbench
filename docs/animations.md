# Workbench Animation System

Workbench implements a "Handmade Hero" style animation system focused on low latency and smooth immediate-mode UI transitions. The core logic resides in `src/core/animation.c` and `src/core/animation.h`.

## Core Mechanisms

### 1. `smooth_value`
Most UI transitions use the `smooth_value` pattern. Instead of complex keyframes, it uses a simple derivative-based approach:
- **Structure**: Contains `current`, `target`, and `speed`.
- **Logic**: Every frame, `current` moves towards `target` at a fixed `speed` (units per second).
- **Files**: `src/core/animation.h`, `src/core/animation.c`

### 2. `animation_state`
A more traditional "timer-based" animation system used for specific durations.
- **Logic**: Tracks `elapsed_ms` vs `duration_ms` and applies an **Easing Function**.
- **Supported Easings**: Linear, Quad (In/Out/InOut), Cubic (In/Out), Exponential (Out).
- **Files**: `src/core/animation.h`, `src/core/animation.c`

---

## Animation Registry

| Feature | Animation Type | Path / State Variable | Description |
| :--- | :--- | :--- | :--- |
| **Global Scrolling** | `smooth_value` | `ui_scroll_state.scroll_v/h` | Provides momentum-based smooth vertical/horizontal scrolling in long lists. |
| **UI Hover** | `smooth_value` | `ui_context.hover_anim` | Global transition state for button/selectable hover highlights. |
| **File Explorer** | `smooth_value` | `explorer_state.selection_anim` | The selection bar slides vertically between files when navigating. |
| **Quick Filter** | `smooth_value` | `quick_filter_state.fade_anim` | The search bar at the bottom fades in and slides up 10px from the bottom border. |
| **Terminal Slide** | `smooth_value` | `terminal_panel_state.anim` | The terminal panel slides up/down when toggled (Ctrl + `). |
| **Terminal Cursor** | `smooth_value` | `terminal_panel_state.cursor_blink` | A smooth opacity fade (0.0 <-> 1.0) rather than a hard binary blink. |
| **Command Palette** | `smooth_value` | `command_palette_state.fade_anim` | The palette fades in and slides down slightly from the top center. |
| **Command Palette** | `smooth_value` | `command_palette_state.scroll` | Smooth vertical scrolling through large file/command results. |
| **Context Menu** | `smooth_value` | `context_menu_state.fade_anim` | Right-click menus fade and shift vertically on appearance. |

---

## Implementation Details

### Frame-Rate Independence
All animations are updated using `ui->dt` (delta time in seconds) to ensure they run at the same speed regardless of the display refresh rate (60Hz, 144Hz, etc.).

### UI Stack Integration
Since Workbench uses an **Immediate Mode UI (IMGUI)**, state is preserved across frames in the persistent component structures (e.g., `explorer_state`) and updated manually at the start of the `Update` or `Render` call for that component.
