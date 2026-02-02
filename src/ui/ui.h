/*
 * ui.h - Immediate Mode UI Framework for Workbench
 *
 * Keyboard-focused UI with style stack, composable widgets,
 * and smooth animations.
 * C99, handmade hero style.
 */

#ifndef UI_H
#define UI_H

#include "../core/animation.h"
#include "../core/theme.h"
#include "../core/types.h"
#include "../platform/platform.h"
#include "../renderer/renderer.h"

/* ===== Configuration ===== */

#define UI_MAX_LAYOUT_STACK 32
#define UI_MAX_STYLE_STACK 32
#define UI_MAX_ID_STACK 32
#define UI_MAX_SCROLL_STACK 16
#define UI_MAX_TEXT_INPUT_SIZE 4096
#define UI_MAX_UNDO_STATES 32
#define UI_MAX_FOCUS_ORDER 256

/* ===== Element ID ===== */

typedef u32 ui_id;

#define UI_ID_NONE 0

/* Generate ID from string (compile-time friendly) */
ui_id UI_GenID(const char *str);

/* Push/pop ID scope for nested components */
void UI_PushID(const char *str);
void UI_PushIDInt(i32 n);
void UI_PopID(void);

/* ===== Style System ===== */

/*
 * Style stack allows applying styles globally or by scope:
 *   UI_PushStyle(UI_STYLE_BUTTON_COLOR, some_color);
 *   ... widgets here use the style ...
 *   UI_PopStyle();
 */

typedef enum {
  /* Colors */
  UI_STYLE_TEXT_COLOR,
  UI_STYLE_BG_COLOR,
  UI_STYLE_BORDER_COLOR,
  UI_STYLE_ACCENT_COLOR,
  UI_STYLE_HOVER_COLOR,
  UI_STYLE_ACTIVE_COLOR,
  UI_STYLE_FOCUS_COLOR,

  /* Dimensions */
  UI_STYLE_PADDING,
  UI_STYLE_SPACING,
  UI_STYLE_BORDER_WIDTH,
  UI_STYLE_BORDER_RADIUS,
  UI_STYLE_FONT_SIZE,
  UI_STYLE_SCROLLBAR_WIDTH,

  /* Layout */
  UI_STYLE_MIN_WIDTH,
  UI_STYLE_MIN_HEIGHT,
  UI_STYLE_MAX_WIDTH,
  UI_STYLE_MAX_HEIGHT,

  UI_STYLE_COUNT
} ui_style_property;

/* Style value union */
typedef union {
  color c;
  f32 f;
  i32 i;
} ui_style_value;

/* Push a style property onto the stack */
void UI_PushStyleColor(ui_style_property prop, color c);
void UI_PushStyleFloat(ui_style_property prop, f32 value);
void UI_PushStyleInt(ui_style_property prop, i32 value);

/* Pop most recent style push */
void UI_PopStyle(void);

/* Pop N style pushes */
void UI_PopStyleN(i32 n);

/* Get current style value */
color UI_GetStyleColor(ui_style_property prop);
f32 UI_GetStyleFloat(ui_style_property prop);
i32 UI_GetStyleInt(ui_style_property prop);

/* ===== Layout System ===== */

typedef enum {
  UI_LAYOUT_VERTICAL,
  UI_LAYOUT_HORIZONTAL,
} ui_layout_direction;

typedef struct {
  ui_layout_direction direction;
  rect bounds;    /* Available space */
  v2i cursor;     /* Current position within bounds */
  i32 spacing;    /* Space between elements */
  i32 max_cross;  /* Max size on cross axis */
  i32 item_count; /* Number of items laid out */
} ui_layout;

/* Begin/end layout containers */
void UI_BeginHorizontal(void);
void UI_EndHorizontal(void);
void UI_BeginVertical(void);
void UI_EndVertical(void);

/* Push custom layout with explicit bounds */
void UI_BeginLayout(ui_layout_direction dir, rect bounds);
void UI_EndLayout(void);

/* Add spacing in current layout direction */
void UI_Spacer(i32 size);

/* ===== Scroll Container ===== */

typedef struct {
  v2f offset;            /* Current scroll position */
  v2f target_offset;     /* Target scroll position (for smooth scrolling) */
  v2f content_size;      /* Total content size */
  v2f view_size;         /* Visible viewport size */
  b32 dragging_v;        /* Dragging vertical scrollbar */
  b32 dragging_h;        /* Dragging horizontal scrollbar */
  f32 drag_start;        /* Mouse position when drag started */
  smooth_value scroll_v; /* Smooth vertical scroll */
  smooth_value scroll_h; /* Smooth horizontal scroll */
} ui_scroll_state;

/* Begin/end scroll container */
void UI_BeginScroll(v2i size, ui_scroll_state *state);
void UI_EndScroll(void);

/* ===== Text Input State ===== */

typedef struct {
  i32 cursor_pos;      /* Cursor position in characters */
  i32 selection_start; /* Selection start (-1 if no selection) */
  i32 selection_end;   /* Selection end */
  f32 cursor_blink;    /* Blink timer */
  b32 has_focus;       /* Currently focused */

  /* Undo/redo */
  struct {
    char text[UI_MAX_TEXT_INPUT_SIZE];
    i32 cursor_pos;
  } undo_stack[UI_MAX_UNDO_STATES];
  i32 undo_index;
  i32 undo_count;
} ui_text_state;

/* ===== Input State ===== */

typedef struct {
  /* Mouse state */
  v2i mouse_pos;
  v2i mouse_delta;
  b32 mouse_down[MOUSE_BUTTON_COUNT];
  b32 mouse_pressed[MOUSE_BUTTON_COUNT];
  b32 mouse_released[MOUSE_BUTTON_COUNT];
  f32 scroll_delta;

  /* Keyboard state */
  b32 key_down[KEY_COUNT];
  b32 key_pressed[KEY_COUNT];
  b32 key_released[KEY_COUNT];
  u32 modifiers; /* MOD_CTRL, MOD_SHIFT, etc */

  /* Text input (UTF-32 codepoint) */
  u32 text_input;
} ui_input;

/* ===== UI Context ===== */

typedef struct ui_context_s {
  /* Renderer */
  render_context *renderer;
  const theme *theme;
  struct font *font;
  struct font *mono_font;

  /* Input */
  ui_input input;

  /* Element state */
  ui_id hot;          /* Mouse is over this element */
  ui_id active;       /* Mouse is pressed on this element */
  ui_id focused;      /* Keyboard focus on this element */
  ui_id last_focused; /* Previous frame's focused element */

  /* Focus navigation */
  ui_id focus_order[UI_MAX_FOCUS_ORDER]; /* Elements in focus order */
  i32 focus_count;
  i32 focus_index;

  /* Layout stack */
  ui_layout layout_stack[UI_MAX_LAYOUT_STACK];
  i32 layout_depth;

  /* ID stack (for nested components) */
  ui_id id_stack[UI_MAX_ID_STACK];
  i32 id_depth;

  /* Style stack */
  struct {
    ui_style_property prop;
    ui_style_value value;
  } style_stack[UI_MAX_STYLE_STACK];
  i32 style_depth;
  ui_style_value style_defaults[UI_STYLE_COUNT];

  /* Scroll container stack */
  struct {
    rect clip;      /* Previous clip rect to restore */
    rect view_rect; /* The scroll view bounds */
    v2i scroll_offset;
    ui_scroll_state *state;
  } scroll_stack[UI_MAX_SCROLL_STACK];
  i32 scroll_depth;

  /* Frame state */
  f32 dt; /* Delta time in seconds */
  u64 frame_count;

  /* Modal state */
  ui_id active_modal;  /* Modal active in this frame */
  ui_id next_modal;    /* Modal to be active next frame */
  ui_id current_modal; /* Currently processing modal (in BeginModal/EndModal) */

  /* Hover animations */
  ui_id hover_anim_id;
  smooth_value hover_anim;

  /* Global window state */
  b32 window_focused;
} ui_context;

/* ===== Core API ===== */

/* Initialize UI context */
void UI_Init(ui_context *ctx, render_context *renderer, const theme *th,
             font *f, font *mono_font);

/* Shutdown UI context */
void UI_Shutdown(ui_context *ctx);

/* Begin/end frame */
void UI_BeginFrame(ui_context *ctx, ui_input *input, f32 dt);
void UI_EndFrame(ui_context *ctx);

/* Get global context (for widget implementations) */
ui_context *UI_GetContext(void);
extern ui_context *g_ui_ctx;

/* ===== Widgets ===== */

/* Button - returns true when clicked */
b32 UI_Button(const char *label);

/* Label - static text display */
void UI_Label(const char *text);

/* Text input - returns true when text changes */
b32 UI_TextInput(char *buffer, i32 buffer_size, const char *placeholder,
                 ui_text_state *state);

/* Process text input logic (shared by widgets) */
b32 UI_ProcessTextInput(ui_text_state *state, char *buffer, i32 buffer_size,
                        ui_input *input);

/* Selectable - returns true when clicked */
b32 UI_Selectable(const char *label, b32 selected);

/* Separator - horizontal divider */
void UI_Separator(void);

/* Panel - reusable panel background (VSCode style) */
void UI_DrawPanel(rect bounds);

/* Modal Management - Blocks interaction outside the modal */
void UI_BeginModal(const char *name);
void UI_EndModal(void);

/* ===== Focus Management ===== */

/* Set focus to specific element */
void UI_SetFocus(ui_id id);

/* Clear all focus */
void UI_ClearFocus(void);

/* Check if element has focus */
b32 UI_HasFocus(ui_id id);

/* Check if element is hot (mouse over) */
b32 UI_IsHot(ui_id id);

/* Check if element is active (mouse down) */
b32 UI_IsActive(ui_id id);

/* Register element in focus order */
void UI_RegisterFocusable(ui_id id);

/* Update hot/active state for an element */
b32 UI_UpdateInteraction(ui_id id, rect bounds);

/* ===== Utility ===== */

/* Get available space in current layout */
rect UI_GetAvailableRect(void);

/* Advance layout by given size */
void UI_AdvanceLayout(i32 width, i32 height);

/* Check if point is in rect */
b32 UI_PointInRect(v2i point, rect r);

/* Get text size */
v2i UI_MeasureText(const char *text, font *f);

#endif /* UI_H */
