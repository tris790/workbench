/*
 * dialog.h - Reusable Dialog Component for Workbench
 *
 * Modal dialogs with input fields and confirmation buttons.
 * C99, handmade hero style.
 */

#ifndef DIALOG_H
#define DIALOG_H

#include "../../core/text.h"
#include "../ui.h"

/* ===== Dialog Types ===== */

typedef enum {
  WB_DIALOG_TYPE_INPUT,   /* Text input dialog (rename, create) */
  WB_DIALOG_TYPE_CONFIRM, /* Confirmation dialog (delete) */
} dialog_type;

typedef enum {
  WB_DIALOG_RESULT_NONE,    /* No action taken */
  WB_DIALOG_RESULT_CONFIRM, /* User confirmed */
  WB_DIALOG_RESULT_CANCEL,  /* User cancelled */
} dialog_result;

/* Dialog configuration */
typedef struct {
  dialog_type type;
  const char *title;
  b32 is_danger; /* Use error/danger styling */

  /* For input dialogs */
  char *input_buffer;
  i32 input_buffer_size;
  ui_text_state *input_state;
  const char *placeholder;

  /* For confirmation dialogs */
  wrapped_text message;
  const char *hint; /* Optional hint text below message */

  /* Button labels (NULL = defaults) */
  const char *confirm_label;
  const char *cancel_label;
} dialog_config;

typedef struct {
  const char *modal_name;
  const char *title;
  i32 preferred_width;
  i32 preferred_height;
  i32 min_width;
  i32 min_height;
  i32 margin_x;
  i32 margin_y;
} dialog_shell_config;

typedef struct {
  rect frame;
  rect header;
  rect content;
  rect footer;
  i32 outer_pad_x;
  i32 footer_pad_y;
  i32 actions_gap_x;
  i32 header_h;
  i32 footer_h;
} dialog_shell_layout;

/* ===== Dialog API ===== */

/* Render a dialog centered in bounds.
 * Returns the result of user interaction (confirm/cancel/none).
 */
dialog_result Dialog_Render(ui_context *ui, rect bounds,
                            const dialog_config *config);

/* Draw a reusable modal shell for larger custom-content dialogs. */
dialog_shell_layout Dialog_BeginShell(ui_context *ui, rect bounds,
                                      const dialog_shell_config *config);

/* End a modal shell started with Dialog_BeginShell. */
void Dialog_EndShell(ui_context *ui, const dialog_shell_layout *layout);

/* Default dialog width */
#define DIALOG_WIDTH 420

#endif /* DIALOG_H */
