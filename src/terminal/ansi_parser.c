/*
 * ansi_parser.c - ANSI escape sequence parser implementation
 *
 * State machine for parsing VT100/ANSI terminal sequences.
 * C99, handmade hero style.
 */

#include "ansi_parser.h"
#include <string.h>

void ANSI_Init(ansi_parser *parser) {
  memset(parser, 0, sizeof(ansi_parser));
  parser->state = ANSI_STATE_GROUND;
}

/* Check if byte is a control character */
static b32 is_control(u8 c) {
  return (c < 0x20 && c != 0x1B) || c == 0x7F;
}

/* Check if byte is a CSI parameter byte (0-9, ;, :) */
static b32 is_csi_param(u8 c) {
  return c >= 0x30 && c <= 0x3F;
}

/* Check if byte is a CSI intermediate byte */
static b32 is_csi_intermediate(u8 c) {
  return c >= 0x20 && c <= 0x2F;
}

/* Check if byte is a CSI final byte */
static b32 is_csi_final(u8 c) {
  return c >= 0x40 && c <= 0x7E;
}

ansi_result ANSI_Parse(ansi_parser *parser, u8 byte) {
  ansi_result result = {.action = ANSI_ACTION_NONE};

  switch (parser->state) {
  case ANSI_STATE_GROUND:
    if (byte == 0x1B) {
      /* ESC - start escape sequence */
      parser->state = ANSI_STATE_ESCAPE;
    } else if (is_control(byte)) {
      /* Control character */
      result.action = ANSI_ACTION_EXECUTE;
      result.data.control = byte;
    } else {
      /* Printable character */
      result.action = ANSI_ACTION_PRINT;
      result.data.character = byte;
    }
    break;

  case ANSI_STATE_ESCAPE:
    if (byte == '[') {
      /* CSI sequence */
      parser->state = ANSI_STATE_CSI;
      parser->param_count = 0;
      parser->current_param = 0;
      parser->has_param = false;
      parser->private_mode = false;
      parser->osc_len = 0;
    } else if (byte == ']') {
      /* OSC sequence */
      parser->state = ANSI_STATE_OSC;
      parser->osc_len = 0;
    } else if (byte == 'P' || byte == '_' || byte == '^' || byte == 'X') {
      /* DCS (P), APC (_), PM (^), SOS (X) - Treat all as ignorable strings */
      parser->state = ANSI_STATE_DCS;
    } else if (byte == 'c') {
      /* RIS - Reset to Initial State */
      result.action = ANSI_ACTION_EXECUTE;
      result.data.control = 'c';
      parser->state = ANSI_STATE_GROUND;
    } else if (byte >= 0x40 && byte <= 0x5F) {
      /* Other ESC sequences (treat as single command) */
      result.action = ANSI_ACTION_EXECUTE;
      result.data.control = byte;
      parser->state = ANSI_STATE_GROUND;
    } else {
      /* Unknown, return to ground */
      parser->state = ANSI_STATE_GROUND;
    }
    break;

  case ANSI_STATE_CSI:
    if (byte == '?') {
      /* Private mode indicator */
      parser->private_mode = true;
    } else if (is_csi_param(byte)) {
      if (byte >= '0' && byte <= '9') {
        parser->current_param = parser->current_param * 10 + (byte - '0');
        parser->has_param = true;
      } else if (byte == ';') {
        /* Parameter separator */
        if (parser->param_count < ANSI_MAX_PARAMS) {
          parser->params[parser->param_count++] =
              parser->has_param ? parser->current_param : 0;
        }
        parser->current_param = 0;
        parser->has_param = false;
      }
    } else if (is_csi_intermediate(byte)) {
      /* Intermediate bytes - ignore for now */
    } else if (is_csi_final(byte)) {
      /* Final byte - CSI sequence complete */
      if (parser->has_param && parser->param_count < ANSI_MAX_PARAMS) {
        parser->params[parser->param_count++] = parser->current_param;
      }

      result.action = ANSI_ACTION_CSI;
      result.data.csi.command = byte;
      result.data.csi.private_mode = parser->private_mode;
      result.data.csi.params = parser->params;
      result.data.csi.param_count = parser->param_count;

      parser->state = ANSI_STATE_GROUND;
    } else {
      /* Invalid, abort sequence */
      parser->state = ANSI_STATE_GROUND;
    }
    break;

  case ANSI_STATE_OSC:
    if (byte == 0x07) {
      /* BEL terminates OSC */
      parser->osc_buffer[parser->osc_len] = '\0';
      result.action = ANSI_ACTION_OSC;
      result.data.osc = parser->osc_buffer;
      parser->state = ANSI_STATE_GROUND;
    } else if (byte == 0x1B) {
      /* ESC might start ST (String Terminator) */
      parser->state = ANSI_STATE_OSC_TEXT;
    } else if (parser->osc_len < ANSI_MAX_OSC - 1) {
      parser->osc_buffer[parser->osc_len++] = byte;
    }
    break;

  case ANSI_STATE_OSC_TEXT:
    if (byte == '\\') {
      /* ST complete (ESC \) */
      parser->osc_buffer[parser->osc_len] = '\0';
      result.action = ANSI_ACTION_OSC;
      result.data.osc = parser->osc_buffer;
    }
    parser->state = ANSI_STATE_GROUND;
    break;

  case ANSI_STATE_DCS:
    if (byte == 0x1B) {
        /* ESC - potentially start of ST */
        parser->state = ANSI_STATE_DCS_ESCAPE;
    } else if (byte == 0x07) {
        /* BEL - terminates */
        result.action = ANSI_ACTION_DCS;
        parser->state = ANSI_STATE_GROUND;
    }
    /* Otherwise consume and ignore byte */
    break;

  case ANSI_STATE_DCS_ESCAPE:
    if (byte == '\\') {
        /* ESC \ (ST) - terminates */
        result.action = ANSI_ACTION_DCS;
        parser->state = ANSI_STATE_GROUND;
    } else {
        /* Not ST, but we consumed ESC. 
           Technically should re-process byte as if after ESC, or just abort.
           For now, just return to ground to avoid stuck state. */
        parser->state = ANSI_STATE_GROUND;
    }
    break;
  }

  return result;
}

i32 ANSI_GetParam(ansi_parser *parser, i32 index, i32 default_val) {
  if (index < 0 || index >= parser->param_count) {
    return default_val;
  }
  i32 val = parser->params[index];
  return val > 0 ? val : default_val;
}
