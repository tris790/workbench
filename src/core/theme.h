/*
 * theme.h - Theme system for Workbench
 *
 * Color palette, spacing, and style definitions.
 * C99, handmade hero style.
 */

#ifndef THEME_H
#define THEME_H

#include "../renderer/renderer.h"

/* ===== Theme Structure ===== */

typedef struct {
  /* Background colors */
  color background;
  color panel;
  color panel_alt;

  /* Text colors */
  color text;
  color text_muted;
  color text_disabled;

  /* Accent colors */
  color accent;
  color accent_hover;
  color accent_active;

  /* UI element colors */
  color border;
  color selection;
  color highlight;

  /* Status colors */
  color success;
  color warning;
  color error;

  /* Spacing (pixels) */
  i32 spacing_xs;
  i32 spacing_sm;
  i32 spacing_md;
  i32 spacing_lg;
  i32 spacing_xl;

  /* Border radius */
  f32 radius_sm;
  f32 radius_md;
  f32 radius_lg;

  /* Font sizes (pixels) */
  i32 font_size_sm;
  i32 font_size_md;
  i32 font_size_lg;
  i32 font_size_xl;
} theme;

/* ===== Theme API ===== */

/* Get default dark theme */
const theme *Theme_GetDefault(void);

/* Initialize theme from config */
void Theme_InitFromConfig(void);

/* Get current active theme */
const theme *Theme_GetCurrent(void);

/* Set active theme */
void Theme_SetCurrent(const theme *t);

/* ===== Color Utilities ===== */

/* Blend two colors (factor: 0.0 = a, 1.0 = b) */
color Color_Blend(color a, color b, f32 factor);

/* Darken color by percentage (0.0 - 1.0) */
color Color_Darken(color c, f32 amount);

/* Lighten color by percentage (0.0 - 1.0) */
color Color_Lighten(color c, f32 amount);

/* Set alpha channel */
color Color_WithAlpha(color c, u8 alpha);

#endif /* THEME_H */
