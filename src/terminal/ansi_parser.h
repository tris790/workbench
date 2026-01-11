/*
 * ansi_parser.h - ANSI escape sequence parser for terminal emulation
 *
 * Parses VT100/ANSI terminal escape sequences.
 * C99, handmade hero style.
 */

#ifndef ANSI_PARSER_H
#define ANSI_PARSER_H

#include "types.h"

/* Parser state machine states */
typedef enum {
  ANSI_STATE_GROUND,   /* Normal character processing */
  ANSI_STATE_ESCAPE,   /* Just saw ESC */
  ANSI_STATE_CSI,      /* Control Sequence Introducer (ESC [) */
  ANSI_STATE_OSC,      /* Operating System Command (ESC ]) */
  ANSI_STATE_OSC_TEXT, /* Reading OSC text */
  ANSI_STATE_DCS,      /* Device Control String (ESC P), APC (ESC _), PM (ESC ^) */
  ANSI_STATE_DCS_ESCAPE /* ESC seen within DCS/APC/PM */
} ansi_state;

/* Action types returned by parser */
typedef enum {
  ANSI_ACTION_NONE,
  ANSI_ACTION_PRINT,     /* Print character */
  ANSI_ACTION_EXECUTE,   /* Control character (CR, LF, etc.) */
  ANSI_ACTION_CSI,       /* CSI sequence complete */
  ANSI_ACTION_OSC,       /* OSC sequence complete */
  ANSI_ACTION_DCS,       /* DCS sequence complete */
} ansi_action;

/* CSI command codes */
typedef enum {
  CSI_CUU = 'A', /* Cursor Up */
  CSI_CUD = 'B', /* Cursor Down */
  CSI_CUF = 'C', /* Cursor Forward */
  CSI_CUB = 'D', /* Cursor Back */
  CSI_CUP = 'H', /* Cursor Position */
  CSI_ED = 'J',  /* Erase Display */
  CSI_EL = 'K',  /* Erase Line */
  CSI_SGR = 'm', /* Select Graphic Rendition */
  CSI_DSR = 'n', /* Device Status Report */
  CSI_SCP = 's', /* Save Cursor Position */
  CSI_RCP = 'u', /* Restore Cursor Position */
  CSI_SM = 'h',  /* Set Mode */
  CSI_RM = 'l',  /* Reset Mode */
  CSI_SD = 'S',  /* Scroll Up */
  CSI_SU = 'T',  /* Scroll Down */
} csi_command;

#define ANSI_MAX_PARAMS 16
#define ANSI_MAX_OSC 256

typedef struct {
  ansi_state state;

  /* CSI parameters */
  i32 params[ANSI_MAX_PARAMS];
  i32 param_count;
  b32 private_mode; /* ? prefix in CSI */

  /* Current parameter being built */
  i32 current_param;
  b32 has_param;

  /* OSC buffer */
  char osc_buffer[ANSI_MAX_OSC];
  i32 osc_len;
} ansi_parser;

/* Result of parsing one byte */
typedef struct {
  ansi_action action;
  union {
    u32 character;    /* For PRINT action */
    u8 control;       /* For EXECUTE action */
    struct {
      char command;
      b32 private_mode;
      i32 *params;
      i32 param_count;
    } csi;            /* For CSI action */
    const char *osc;  /* For OSC action */
  } data;
} ansi_result;

/* Initialize parser state */
void ANSI_Init(ansi_parser *parser);

/* Parse one byte, returns action to take */
ansi_result ANSI_Parse(ansi_parser *parser, u8 byte);

/* Get CSI parameter with default value */
i32 ANSI_GetParam(ansi_parser *parser, i32 index, i32 default_val);

#endif /* ANSI_PARSER_H */
