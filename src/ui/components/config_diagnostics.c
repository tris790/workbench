#include "config_diagnostics.h"
#include "../../config/config.h"
#include "../../config/config_internal.h"
#include "../../core/input.h"
#include "../../core/theme.h"
#include "scroll_container.h"
#include <stdio.h>

void ConfigDiagnostics_Render(ui_context *ui, rect bounds,
                              layout_state *layout) {
  if (!layout->show_config_diagnostics)
    return;

  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;

  /* Dim background */
  color dim = Color_WithAlpha(th->background, 200);
  Render_DrawRect(ctx, bounds, dim);

  UI_BeginModal("ConfigDiagnostics");

  /* Dialog size */
  i32 dialog_w = 600;
  i32 dialog_h = 500;
  rect dialog = {bounds.x + (bounds.w - dialog_w) / 2,
                 bounds.y + (bounds.h - dialog_h) / 2, dialog_w, dialog_h};

  UI_DrawPanel(dialog);

  /* Header */
  i32 header_h = 44;
  rect header_rect = {dialog.x, dialog.y, dialog.w, header_h};
  v2i title_pos = {header_rect.x + th->spacing_lg,
                   header_rect.y +
                       (header_h - Font_GetLineHeight(ui->font)) / 2};
  Render_DrawText(ctx, title_pos, "Configuration Diagnostics", ui->font,
                  th->text);

  rect sep = {dialog.x, dialog.y + header_h, dialog.w, 1};
  Render_DrawRect(ctx, sep, Color_WithAlpha(th->border, 100));

  /* Actions (Footer) */
  i32 footer_h = 56;
  rect footer_rect = {dialog.x, dialog.y + dialog.h - footer_h, dialog.w,
                      footer_h};

  /* Render footer background separator */
  rect footer_sep = {footer_rect.x, footer_rect.y, footer_rect.w, 1};
  Render_DrawRect(ctx, footer_sep, Color_WithAlpha(th->border, 50));

  /* Content Area - fixed portion for path */
  rect path_rect = {
      dialog.x + th->spacing_lg, dialog.y + header_h + th->spacing_md,
      dialog.w - th->spacing_lg * 2, Font_GetLineHeight(ui->font)};

  /* Config Path */
  char path_buf[1024];
  snprintf(path_buf, sizeof(path_buf), "File: %s",
           Config_GetPath() ? Config_GetPath() : "Unknown");
  Render_DrawText(ctx, (v2i){path_rect.x, path_rect.y}, path_buf, ui->font,
                  th->text);

  /* Scrollable content area - below path */
  i32 path_area_h = Font_GetLineHeight(ui->font) + 12;
  rect scroll_rect = {dialog.x + th->spacing_lg,
                      dialog.y + header_h + th->spacing_md + path_area_h,
                      dialog.w - th->spacing_lg * 2,
                      dialog.h - header_h - footer_h - th->spacing_md * 2 -
                          path_area_h};

  /* Update scroll container - handles input */
  scroll_container_state *scroll = &layout->diagnostic_scroll;
  ScrollContainer_Update(scroll, ui, scroll_rect);

  /* Set clip rect for scrollable content */
  Render_SetClipRect(ctx, scroll_rect);

  /* Calculate content layout with scroll offset */
  i32 scroll_offset_y = (i32)ScrollContainer_GetOffsetY(scroll);
  rect content_bounds = {scroll_rect.x, scroll_rect.y - scroll_offset_y,
                         scroll_rect.w - SCROLL_SCROLLBAR_GUTTER,
                         scroll_rect.h * 10}; /* Large height for content */

  UI_BeginLayout(WB_UI_LAYOUT_VERTICAL, content_bounds);

  /* Errors Section */
  i32 diag_count = Config_GetDiagnosticCount();
  if (diag_count > 0) {
    UI_PushStyleColor(WB_UI_STYLE_TEXT_COLOR, th->error);
    UI_Label("Errors:");

    for (i32 i = 0; i < diag_count; i++) {
      char diag_buf[1024];
      snprintf(diag_buf, sizeof(diag_buf), "  - %s",
               Config_GetDiagnosticMessage(i));
      UI_Label(diag_buf);
    }
    UI_PopStyle();
    UI_Spacer(16);
  }

  /* Loaded Values Section */
  UI_PushStyleColor(WB_UI_STYLE_TEXT_COLOR, th->accent);
  UI_Label("Loaded Values:");
  UI_PopStyle();
  UI_Spacer(4);

  i32 entry_count = Config_GetEntryCount();
  for (i32 i = 0; i < entry_count; i++) {
    ConfigEntry *entry = Config_GetEntry(i);
    if (!entry)
      continue;

    char entry_buf[3072];
    const char *type_str = "unknown";
    char val_buf[2048];

    switch (entry->type) {
    case WB_CONFIG_TYPE_BOOL:
      type_str = "bool";
      snprintf(val_buf, sizeof(val_buf), "%s",
               entry->value.bool_val ? "true" : "false");
      break;
    case WB_CONFIG_TYPE_I64:
      type_str = "i64";
      snprintf(val_buf, sizeof(val_buf), "%lld",
               (long long)entry->value.i64_val);
      break;
    case WB_CONFIG_TYPE_F64:
      type_str = "f64";
      snprintf(val_buf, sizeof(val_buf), "%.2f", entry->value.f64_val);
      break;
    case WB_CONFIG_TYPE_STRING:
      type_str = "string";
      snprintf(val_buf, sizeof(val_buf), "\"%s\"", entry->value.string_val);
      break;
    }

    snprintf(entry_buf, sizeof(entry_buf), "  %-24s (%-6s) = %s", entry->key,
             type_str, val_buf);
    UI_Label(entry_buf);
  }

  /* Get content height from layout before ending it */
  ui_context *ui_ctx = UI_GetContext();
  ui_layout *current_layout = &ui_ctx->layout_stack[ui_ctx->layout_depth - 1];
  f32 content_height =
      (f32)(current_layout->cursor.y - current_layout->bounds.y);

  UI_EndLayout();

  /* Update scroll container with measured content size */
  ScrollContainer_SetContentSize(scroll, content_height);

  /* Reset clip and render scrollbar */
  Render_ResetClipRect(ctx);
  ScrollContainer_RenderScrollbar(scroll, ui);

  /* Footer Buttons - Right aligned */
  i32 btn_pad = th->spacing_sm * 2;

  /* Measure buttons */
  i32 close_w = UI_MeasureText("Close", ui->font).x + btn_pad;
  if (close_w < 80)
    close_w = 80; /* Min width for aesthetics */

  i32 open_w = UI_MeasureText("Open Config File", ui->font).x + btn_pad;
  i32 reload_w = UI_MeasureText("Reload", ui->font).x + btn_pad;

  i32 btn_h = 32;
  i32 btn_y = footer_rect.y + (footer_rect.h - btn_h) / 2;

  /* Layout from right to left */
  i32 current_x = footer_rect.x + footer_rect.w - th->spacing_lg;

  /* Close Button */
  current_x -= close_w;
  rect close_rect = {current_x, btn_y, close_w, btn_h};
  UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, close_rect);
  if (UI_Button("Close")) {
    layout->show_config_diagnostics = false;
    UI_EndModal();
    Input_PopFocus();
  }
  UI_EndLayout();

  /* Open File Button */
  current_x -= (open_w + th->spacing_md);
  rect open_rect = {current_x, btn_y, open_w, btn_h};
  UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, open_rect);
  if (UI_Button("Open Config File")) {
    const char *path = Config_GetPath();
    if (path) {
      Platform_OpenFile(path);
    }
  }
  UI_EndLayout();

  /* Reload Button */
  current_x -= (reload_w + th->spacing_md);
  rect reload_rect = {current_x, btn_y, reload_w, btn_h};
  UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, reload_rect);
  if (UI_Button("Reload")) {
    Config_Reload();
  }
  UI_EndLayout();

  /* Handle Escape / Return to close */
  if (ui->input.key_pressed[WB_KEY_ESCAPE] || ui->input.key_pressed[WB_KEY_RETURN]) {
    layout->show_config_diagnostics = false;
    UI_EndModal();
    Input_PopFocus();
  }

  UI_EndModal();
}
