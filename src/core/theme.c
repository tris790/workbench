/*
 * theme.c - Theme system implementation for Workbench
 *
 * C99, handmade hero style.
 */

#include "theme.h"

/* ===== Default Dark Theme ===== */

/* Catppuccin Mocha inspired */
static const theme g_dark_theme = {
    /* Background colors */
    .background = {30, 30, 46, 255}, /* #1E1E2E */
    .panel = {36, 36, 54, 255},      /* #24243E */
    .panel_alt = {43, 43, 61, 255},  /* #2B2B3D */

    /* Text colors */
    .text = {205, 214, 244, 255},        /* #CDD6F4 */
    .text_muted = {147, 153, 178, 255},  /* #9399B2 */
    .text_disabled = {88, 91, 112, 255}, /* #585B70 */

    /* Accent colors */
    .accent = {137, 180, 250, 255},       /* #89B4FA (blue) */
    .accent_hover = {116, 169, 250, 255}, /* #74A9FA */
    .accent_active = {96, 158, 250, 255}, /* #609EFA */

    /* UI element colors */
    .border = {69, 71, 90, 255},      /* #45475A */
    .selection = {88, 91, 112, 128},  /* #585B70 (50% alpha) */
    .highlight = {249, 226, 175, 64}, /* #F9E2AF (25% alpha) */

    /* Status colors */
    .success = {166, 227, 161, 255}, /* #A6E3A1 (green) */
    .warning = {249, 226, 175, 255}, /* #F9E2AF (yellow) */
    .error = {243, 139, 168, 255},   /* #F38BA8 (red) */

    /* Spacing (pixels) */
    .spacing_xs = 4,
    .spacing_sm = 8,
    .spacing_md = 12,
    .spacing_lg = 16,
    .spacing_xl = 24,

    /* Border radius */
    .radius_sm = 4.0f,
    .radius_md = 6.0f,
    .radius_lg = 8.0f,

    /* Font sizes */
    .font_size_sm = 12,
    .font_size_md = 14,
    .font_size_lg = 16,
    .font_size_xl = 20};

/* ===== Current Theme State ===== */

static const theme *g_current_theme = &g_dark_theme;

/* ===== Theme API ===== */

const theme *Theme_GetDefault(void) { return &g_dark_theme; }

const theme *Theme_GetCurrent(void) { return g_current_theme; }

void Theme_SetCurrent(const theme *t) {
  g_current_theme = t ? t : &g_dark_theme;
}

/* ===== Color Utilities ===== */

static inline u8 ClampU8(i32 value) {
  if (value < 0)
    return 0;
  if (value > 255)
    return 255;
  return (u8)value;
}

color Color_Blend(color a, color b, f32 factor) {
  if (factor <= 0.0f)
    return a;
  if (factor >= 1.0f)
    return b;

  f32 inv = 1.0f - factor;
  return (color){ClampU8((i32)(a.r * inv + b.r * factor)),
                 ClampU8((i32)(a.g * inv + b.g * factor)),
                 ClampU8((i32)(a.b * inv + b.b * factor)),
                 ClampU8((i32)(a.a * inv + b.a * factor))};
}

color Color_Darken(color c, f32 amount) {
  f32 factor = 1.0f - amount;
  return (color){ClampU8((i32)(c.r * factor)), ClampU8((i32)(c.g * factor)),
                 ClampU8((i32)(c.b * factor)), c.a};
}

color Color_Lighten(color c, f32 amount) {
  return (color){ClampU8(c.r + (i32)((255 - c.r) * amount)),
                 ClampU8(c.g + (i32)((255 - c.g) * amount)),
                 ClampU8(c.b + (i32)((255 - c.b) * amount)), c.a};
}

color Color_WithAlpha(color c, u8 alpha) {
  return (color){c.r, c.g, c.b, alpha};
}
