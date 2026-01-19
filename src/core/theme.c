/*
 * theme.c - Theme system implementation for Workbench
 *
 * C99, handmade hero style.
 */

#include "theme.h"

/* ===== Default Dark Theme ===== */

/* Catppuccin Mocha inspired */
static theme g_dark_theme = {
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

/* ===== Theme config integration ===== */
#include "../config/config.h"
#include <string.h>

static int HexDigit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static b32 ParseColor(const char *hex, color *out) {
  if (!hex || hex[0] != '#')
    return false;
  hex++;
  size_t len = strlen(hex);
  if (len != 6 && len != 8)
    return false;

  int r1 = HexDigit(hex[0]);
  int r2 = HexDigit(hex[1]);
  int g1 = HexDigit(hex[2]);
  int g2 = HexDigit(hex[3]);
  int b1 = HexDigit(hex[4]);
  int b2 = HexDigit(hex[5]);

  if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
    return false;

  out->r = (u8)(r1 * 16 + r2);
  out->g = (u8)(g1 * 16 + g2);
  out->b = (u8)(b1 * 16 + b2);
  out->a = 255;

  if (len == 8) {
    int a1 = HexDigit(hex[6]);
    int a2 = HexDigit(hex[7]);
    if (a1 < 0 || a2 < 0)
      return false;
    out->a = (u8)(a1 * 16 + a2);
  }
  return true;
}

void Theme_InitFromConfig(void) {
  const char *val;
  color c;

  if ((val = Config_GetString("theme.background", NULL)) && ParseColor(val, &c))
    g_dark_theme.background = c;
  if ((val = Config_GetString("theme.panel", NULL)) && ParseColor(val, &c))
    g_dark_theme.panel = c;
  if ((val = Config_GetString("theme.text", NULL)) && ParseColor(val, &c))
    g_dark_theme.text = c;
  if ((val = Config_GetString("theme.accent", NULL)) && ParseColor(val, &c)) {
    g_dark_theme.accent = c;
    /* Auto-generate hover/active states for accent */
    g_dark_theme.accent_hover = Color_Lighten(c, 0.2f);
    g_dark_theme.accent_active = Color_Darken(c, 0.2f);
  }
}

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
