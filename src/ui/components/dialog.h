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
  DIALOG_TYPE_INPUT,   /* Text input dialog (rename, create) */
  DIALOG_TYPE_CONFIRM, /* Confirmation dialog (delete) */
} dialog_type;

typedef enum {
  DIALOG_RESULT_NONE,    /* No action taken */
  DIALOG_RESULT_CONFIRM, /* User confirmed */
  DIALOG_RESULT_CANCEL,  /* User cancelled */
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

/* ===== Dialog API ===== */

/* Render a dialog centered in bounds.
 * Returns the result of user interaction (confirm/cancel/none).
 */
dialog_result Dialog_Render(ui_context *ui, rect bounds,
                            const dialog_config *config);

/* Default dialog width */
#define DIALOG_WIDTH 420

#endif /* DIALOG_H */
