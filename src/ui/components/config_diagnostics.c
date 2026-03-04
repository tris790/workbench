#include "config_diagnostics.h"
#include "../../config/config.h"
#include "../../config/config_internal.h"
#include "../../core/input.h"
#include "../../core/theme.h"
#include "dialog.h"
#include "scroll_container.h"
#include <stdio.h>
#include <string.h>

#define CONFIG_DIAGNOSTICS_MIN_WIDTH 560
#define CONFIG_DIAGNOSTICS_MIN_HEIGHT 360
#define CONFIG_DIAGNOSTICS_MAX_WIDTH 980
#define CONFIG_DIAGNOSTICS_MAX_HEIGHT 760
#define CONFIG_DIAGNOSTICS_MARGIN_X 32
#define CONFIG_DIAGNOSTICS_MARGIN_Y 36

static void RenderTextWithEllipsis(ui_context *ui, font *f, v2i pos,
                                   const char *text, i32 max_width,
                                   color text_color, char *scratch,
                                   usize scratch_size) {
  usize len;
  i32 ellipsis_w;

  if (!text || !f || !scratch || scratch_size == 0 || max_width <= 0) {
    return;
  }

  if (Font_MeasureWidth(f, text) <= max_width) {
    Render_DrawText(ui->renderer, pos, text, f, text_color);
    return;
  }

  strncpy(scratch, text, scratch_size - 1);
  scratch[scratch_size - 1] = '\0';

  ellipsis_w = Font_MeasureWidth(f, "...");
  len = strlen(scratch);
  while (len > 0 && Font_MeasureWidth(f, scratch) > max_width - ellipsis_w) {
    scratch[--len] = '\0';
  }

  if (len == 0) {
    if (ellipsis_w <= max_width) {
      Render_DrawText(ui->renderer, pos, "...", f, text_color);
    }
    return;
  }

  strncat(scratch, "...", scratch_size - strlen(scratch) - 1);
  Render_DrawText(ui->renderer, pos, scratch, f, text_color);
}

static i32 RenderWrappedText(ui_context *ui, memory_arena *arena,
                             const char *text, font *f, rect bounds,
                             color text_color) {
  i32 line_height;
  wrapped_text wrapped;
  i32 y;

  if (!text || !f || bounds.w <= 0) {
    return 0;
  }

  line_height = Font_GetLineHeight(f);
  wrapped = Text_Wrap(arena, text, f, bounds.w);
  if (!wrapped.lines || wrapped.count <= 0) {
    Render_DrawText(ui->renderer, (v2i){bounds.x, bounds.y}, text, f,
                    text_color);
    return line_height;
  }

  y = bounds.y;
  for (i32 i = 0; i < wrapped.count; ++i) {
    Render_DrawText(ui->renderer, (v2i){bounds.x, y}, wrapped.lines[i], f,
                    text_color);
    y += line_height;
  }

  return wrapped.count * line_height;
}

static void FormatConfigValue(const ConfigEntry *entry, char *value_buf,
                              usize value_buf_size, const char **type_str) {
  *type_str = "unknown";
  value_buf[0] = '\0';

  switch (entry->type) {
  case WB_CONFIG_TYPE_BOOL:
    *type_str = "bool";
    snprintf(value_buf, value_buf_size, "%s",
             entry->value.bool_val ? "true" : "false");
    break;
  case WB_CONFIG_TYPE_I64:
    *type_str = "i64";
    snprintf(value_buf, value_buf_size, "%lld",
             (long long)entry->value.i64_val);
    break;
  case WB_CONFIG_TYPE_F64:
    *type_str = "f64";
    snprintf(value_buf, value_buf_size, "%.2f", entry->value.f64_val);
    break;
  case WB_CONFIG_TYPE_STRING:
    *type_str = "string";
    snprintf(value_buf, value_buf_size, "\"%s\"", entry->value.string_val);
    break;
  }
}

void ConfigDiagnostics_Render(ui_context *ui, rect bounds,
                              layout_state *layout) {
  render_context *ctx;
  const theme *th;
  temporary_memory temp;
  dialog_shell_config shell_config;
  dialog_shell_layout shell;
  rect meta_rect;
  rect body_rect;
  i32 line_height;
  i32 meta_height;
  i32 body_gap;
  i32 diag_count;
  i32 entry_count;
  char path_buf[FS_MAX_PATH + 16];
  char path_draw_buf[FS_MAX_PATH + 16];
  char error_status[32];
  char values_status[32];
  i32 error_status_w;
  i32 values_status_w;
  i32 right_group_w;
  i32 right_gap;
  i32 path_max_w;
  scroll_container_state *scroll;
  i32 scroll_offset_y;
  i32 content_x;
  i32 content_y;
  i32 content_w;
  i32 content_start_y;
  i32 content_height;
  b32 close_modal = false;
  font *code_font;
  i32 btn_h = 36;
  i32 btn_pad;

  if (!layout->show_config_diagnostics) {
    return;
  }

  ctx = ui->renderer;
  th = ui->theme;
  temp = BeginTemporaryMemory(layout->arena);
  line_height = Font_GetLineHeight(ui->font);
  body_gap = th->spacing_md;
  diag_count = Config_GetDiagnosticCount();
  entry_count = Config_GetEntryCount();
  code_font = ui->mono_font ? ui->mono_font : ui->font;
  btn_pad = th->spacing_lg * 2;

  shell_config = (dialog_shell_config){
      .modal_name = "ConfigDiagnostics",
      .title = "Configuration Diagnostics",
      .preferred_width = Min(CONFIG_DIAGNOSTICS_MAX_WIDTH,
                             Max(CONFIG_DIAGNOSTICS_MIN_WIDTH,
                                 bounds.w - CONFIG_DIAGNOSTICS_MARGIN_X * 2)),
      .preferred_height = Min(
          CONFIG_DIAGNOSTICS_MAX_HEIGHT,
          Max(CONFIG_DIAGNOSTICS_MIN_HEIGHT,
              bounds.h - CONFIG_DIAGNOSTICS_MARGIN_Y * 2)),
      .min_width = CONFIG_DIAGNOSTICS_MIN_WIDTH,
      .min_height = CONFIG_DIAGNOSTICS_MIN_HEIGHT,
      .margin_x = CONFIG_DIAGNOSTICS_MARGIN_X,
      .margin_y = CONFIG_DIAGNOSTICS_MARGIN_Y,
  };
  shell = Dialog_BeginShell(ui, bounds, &shell_config);

  meta_height = line_height + th->spacing_md;
  if (meta_height < 32) {
    meta_height = 32;
  }

  meta_rect = (rect){shell.content.x, shell.content.y, shell.content.w,
                     meta_height};
  body_rect = (rect){shell.content.x, shell.content.y + meta_height + body_gap,
                     shell.content.w,
                     shell.content.h - meta_height - body_gap};
  if (body_rect.h < 0) {
    body_rect.h = 0;
  }

  snprintf(path_buf, sizeof(path_buf), "File: %s",
           Config_GetPath() ? Config_GetPath() : "Unknown");
  snprintf(error_status, sizeof(error_status), "%d error%s", diag_count,
           diag_count == 1 ? "" : "s");
  snprintf(values_status, sizeof(values_status), "%d loaded value%s",
           entry_count, entry_count == 1 ? "" : "s");

  error_status_w = Font_MeasureWidth(ui->font,
                                     diag_count > 0 ? error_status : "No errors");
  values_status_w = Font_MeasureWidth(ui->font, values_status);
  right_gap = th->spacing_lg;
  right_group_w = error_status_w + right_gap + values_status_w;
  path_max_w = meta_rect.w - right_group_w - th->spacing_lg;
  if (path_max_w < 0) {
    path_max_w = 0;
  }

  RenderTextWithEllipsis(
      ui, ui->font,
      (v2i){meta_rect.x, meta_rect.y + (meta_rect.h - line_height) / 2},
      path_buf, path_max_w, th->text, path_draw_buf, sizeof(path_draw_buf));

  if (diag_count > 0) {
    Render_DrawText(
        ctx,
        (v2i){meta_rect.x + meta_rect.w - right_group_w,
              meta_rect.y + (meta_rect.h - line_height) / 2},
        error_status, ui->font, th->error);
  } else {
    Render_DrawText(
        ctx,
        (v2i){meta_rect.x + meta_rect.w - right_group_w,
              meta_rect.y + (meta_rect.h - line_height) / 2},
        "No errors", ui->font, th->success);
  }

  Render_DrawText(
      ctx,
      (v2i){meta_rect.x + meta_rect.w - values_status_w,
            meta_rect.y + (meta_rect.h - line_height) / 2},
      values_status, ui->font, th->text_muted);

  Render_DrawRect(
      ctx,
      (rect){meta_rect.x, meta_rect.y + meta_rect.h + th->spacing_xs / 2,
             meta_rect.w, 1},
      Color_WithAlpha(th->border, 70));

  scroll = &layout->diagnostic_scroll;
  ScrollContainer_Update(scroll, ui, body_rect);

  Render_SetClipRect(ctx, body_rect);

  scroll_offset_y = (i32)ScrollContainer_GetOffsetY(scroll);
  content_x = body_rect.x;
  content_y = body_rect.y - scroll_offset_y;
  content_w = body_rect.w - SCROLL_SCROLLBAR_GUTTER;
  if (content_w < 0) {
    content_w = 0;
  }
  content_start_y = content_y;

  if (diag_count > 0) {
    Render_DrawText(ctx, (v2i){content_x, content_y}, "Errors", ui->font,
                    th->error);
    content_y += line_height + th->spacing_sm;

    for (i32 i = 0; i < diag_count; ++i) {
      char diag_buf[544];
      const char *message = Config_GetDiagnosticMessage(i);
      i32 diag_height;

      snprintf(diag_buf, sizeof(diag_buf), "- %s", message ? message : "");
      diag_height =
          RenderWrappedText(ui, layout->arena, diag_buf, ui->font,
                            (rect){content_x, content_y, content_w, 0},
                            th->error);
      content_y += diag_height + th->spacing_sm;
    }

    content_y += th->spacing_lg;
  }

  Render_DrawText(ctx, (v2i){content_x, content_y}, "Loaded Values", ui->font,
                  th->accent);
  content_y += line_height + th->spacing_md;

  if (entry_count == 0) {
    Render_DrawText(ctx, (v2i){content_x, content_y}, "No values loaded.",
                    ui->font, th->text_muted);
    content_y += line_height;
  } else {
    i32 col_gap = th->spacing_md;
    i32 type_w = 72;
    i32 key_w = Clamp((content_w * 34) / 100, 120, 280);
    i32 min_value_w = 120;

    if (content_w - (col_gap * 2) - type_w - key_w < min_value_w) {
      type_w = 56;
      key_w = content_w - (col_gap * 2) - type_w - min_value_w;
      if (key_w < 96) {
        key_w = 96;
      }
    }

    for (i32 i = 0; i < entry_count; ++i) {
      ConfigEntry *entry = Config_GetEntry(i);
      char value_buf[2048];
      const char *type_str;
      char key_buf[256];
      i32 key_x;
      i32 type_x;
      i32 value_x;
      i32 value_w;
      i32 value_height;
      i32 row_height;

      if (!entry) {
        continue;
      }

      key_x = content_x;
      type_x = key_x + key_w + col_gap;
      value_x = type_x + type_w + col_gap;
      value_w = content_w - (value_x - content_x);
      if (value_w < 64) {
        value_w = 64;
      }

      FormatConfigValue(entry, value_buf, sizeof(value_buf), &type_str);
      value_height =
          RenderWrappedText(ui, layout->arena, value_buf, code_font,
                            (rect){value_x, content_y, value_w, 0}, th->text);
      row_height = Max(line_height, value_height);

      RenderTextWithEllipsis(ui, code_font, (v2i){key_x, content_y}, entry->key,
                             key_w, th->text, key_buf, sizeof(key_buf));
      Render_DrawText(ctx, (v2i){type_x, content_y}, type_str, ui->font,
                      th->text_muted);

      content_y += row_height + th->spacing_sm;
      Render_DrawRect(ctx,
                      (rect){content_x, content_y, content_w, 1},
                      Color_WithAlpha(th->border, 40));
      content_y += th->spacing_sm;
    }
  }

  content_height = content_y - content_start_y;
  if (content_height < body_rect.h) {
    content_height = body_rect.h;
  }
  ScrollContainer_SetContentSize(scroll, (f32)content_height);

  Render_ResetClipRect(ctx);
  ScrollContainer_RenderScrollbar(scroll, ui);

  {
    const char *close_label = "Close";
    const char *open_label = "Open Config File";
    const char *reload_label = "Reload";
    i32 close_w = Max(96, UI_MeasureText(close_label, ui->font).x + btn_pad);
    i32 open_w = Max(96, UI_MeasureText(open_label, ui->font).x + btn_pad);
    i32 reload_w = Max(96, UI_MeasureText(reload_label, ui->font).x + btn_pad);
    i32 btn_y = shell.footer.y + (shell.footer.h - btn_h) / 2;
    i32 current_x = shell.footer.x + shell.footer.w - shell.outer_pad_x;

    current_x -= close_w;
    UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL,
                   (rect){current_x, btn_y, close_w, btn_h});
    UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, close_w);
    UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, btn_h);
    UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, Color_WithAlpha(th->panel_alt, 220));
    UI_PushStyleColor(WB_UI_STYLE_HOVER_COLOR, Color_Lighten(th->panel_alt, 0.1f));
    UI_PushStyleColor(WB_UI_STYLE_ACTIVE_COLOR,
                      Color_Lighten(th->panel_alt, 0.18f));
    if (UI_Button(close_label)) {
      close_modal = true;
    }
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_EndLayout();

    current_x -= open_w + shell.actions_gap_x;
    UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL,
                   (rect){current_x, btn_y, open_w, btn_h});
    UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, open_w);
    UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, btn_h);
    UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, Color_WithAlpha(th->panel_alt, 220));
    UI_PushStyleColor(WB_UI_STYLE_HOVER_COLOR, Color_Lighten(th->panel_alt, 0.1f));
    UI_PushStyleColor(WB_UI_STYLE_ACTIVE_COLOR,
                      Color_Lighten(th->panel_alt, 0.18f));
    if (UI_Button(open_label)) {
      const char *path = Config_GetPath();
      if (path) {
        Platform_OpenFile(path);
      }
    }
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_EndLayout();

    current_x -= reload_w + shell.actions_gap_x;
    UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL,
                   (rect){current_x, btn_y, reload_w, btn_h});
    UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, reload_w);
    UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, btn_h);
    UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, th->accent);
    UI_PushStyleColor(WB_UI_STYLE_HOVER_COLOR, th->accent_hover);
    UI_PushStyleColor(WB_UI_STYLE_ACTIVE_COLOR, th->accent_active);
    if (UI_Button(reload_label)) {
      Config_Reload();
    }
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_PopStyle();
    UI_EndLayout();
  }

  if (ui->input.key_pressed[WB_KEY_ESCAPE]) {
    close_modal = true;
  }

  if (close_modal) {
    layout->show_config_diagnostics = false;
    Input_PopFocus();
  }

  Dialog_EndShell(ui, &shell);
  EndTemporaryMemory(temp);
}
