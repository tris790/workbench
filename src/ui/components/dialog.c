/*
 * dialog.c - Reusable Dialog Component Implementation
 *
 * C99, handmade hero style.
 */

#include "dialog.h"
#include "theme.h"

static i32 Dialog_GetConfirmBodyHeight(ui_context *ui,
                                       const dialog_config *config) {
  i32 icon_size = 20;
  i32 text_h = Font_GetLineHeight(ui->font);

  if (config->message.lines) {
    text_h = Text_GetWrappedHeight(config->message.count, ui->font);
  }

  if (config->hint) {
    text_h += ui->theme->spacing_md + Font_GetLineHeight(ui->font);
  }

  return (text_h > icon_size) ? text_h : icon_size;
}

static void Dialog_RenderConfirmBody(ui_context *ui, rect body_rect,
                                     const dialog_config *config) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  i32 icon_size = 20;
  rect icon_rect = {body_rect.x, body_rect.y, icon_size, icon_size};
  i32 text_x = body_rect.x + icon_size + th->spacing_lg;
  i32 text_y = body_rect.y;

  Render_DrawRectRounded(ctx, icon_rect, 4.0f, th->error);

  if (config->message.lines) {
    v2i msg_pos = {text_x, text_y};
    for (i32 i = 0; i < config->message.count; i++) {
      Render_DrawText(ctx, msg_pos, config->message.lines[i], ui->font,
                      th->text);
      msg_pos.y += Font_GetLineHeight(ui->font);
    }
    text_y = msg_pos.y;
  } else {
    v2i msg_pos = {text_x, text_y};
    Render_DrawText(ctx, msg_pos, "Are you sure?", ui->font, th->text);
    text_y += Font_GetLineHeight(ui->font);
  }

  if (config->hint) {
    v2i hint_pos = {text_x, text_y + th->spacing_md};
    Render_DrawText(ctx, hint_pos, config->hint, ui->font, th->text_muted);
  }
}

static void Dialog_RenderInputBody(rect body_rect, const dialog_config *config) {
  rect input_rect = {body_rect.x, body_rect.y, body_rect.w, 36};

  UI_PushStyleInt(WB_UI_STYLE_PADDING, 8);
  UI_BeginLayout(WB_UI_LAYOUT_VERTICAL, input_rect);
  if (config->input_buffer && config->input_state) {
    UI_TextInput(config->input_buffer, config->input_buffer_size,
                 config->placeholder ? config->placeholder : "Enter text...",
                 config->input_state);
  }
  UI_EndLayout();
  UI_PopStyle();
}

dialog_shell_layout Dialog_BeginShell(ui_context *ui, rect bounds,
                                      const dialog_shell_config *config) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  dialog_shell_layout layout = {0};
  i32 header_h = 52;
  i32 footer_pad_y = th->spacing_lg;
  i32 footer_h = 36 + (footer_pad_y * 2);
  i32 outer_pad_x = th->spacing_xl;
  i32 body_pad_y = th->spacing_lg;
  i32 margin_x = 32;
  i32 margin_y = 36;
  i32 preferred_w = DIALOG_WIDTH;
  i32 preferred_h = header_h + footer_h + 160;
  i32 min_w = DIALOG_WIDTH;
  i32 min_h = header_h + footer_h + 96;
  const char *title = "Dialog";
  const char *modal_name = "DialogShell";
  color backdrop = {0, 0, 0, 180};
  i32 avail_w;
  i32 avail_h;
  i32 dialog_w;
  i32 dialog_h;
  rect border;
  rect header_sep;
  rect footer_sep;

  if (config) {
    if (config->title) {
      title = config->title;
    }
    if (config->modal_name) {
      modal_name = config->modal_name;
    }
    if (config->margin_x > 0) {
      margin_x = config->margin_x;
    }
    if (config->margin_y > 0) {
      margin_y = config->margin_y;
    }
    if (config->preferred_width > 0) {
      preferred_w = config->preferred_width;
    }
    if (config->preferred_height > 0) {
      preferred_h = config->preferred_height;
    }
    if (config->min_width > 0) {
      min_w = config->min_width;
    }
    if (config->min_height > 0) {
      min_h = config->min_height;
    }
  }

  UI_BeginModal(modal_name);

  Render_DrawRect(ctx, bounds, backdrop);

  avail_w = Max(1, bounds.w - margin_x * 2);
  avail_h = Max(1, bounds.h - margin_y * 2);
  dialog_w = Min(avail_w, Max(min_w, preferred_w));
  dialog_h = Min(avail_h, Max(min_h, preferred_h));

  layout.frame = (rect){bounds.x + (bounds.w - dialog_w) / 2,
                        bounds.y + (bounds.h - dialog_h) / 2, dialog_w,
                        dialog_h};
  layout.header_h = header_h;
  layout.footer_h = footer_h;
  layout.outer_pad_x = outer_pad_x;
  layout.footer_pad_y = footer_pad_y;
  layout.actions_gap_x = th->spacing_md;

  border = (rect){layout.frame.x - 1, layout.frame.y - 1, layout.frame.w + 2,
                  layout.frame.h + 2};
  Render_DrawRectRounded(ctx, border, th->radius_md + 1, th->border);
  UI_DrawPanel(layout.frame);

  layout.header = (rect){layout.frame.x, layout.frame.y, layout.frame.w,
                         header_h};
  Render_DrawText(
      ctx,
      (v2i){layout.header.x + outer_pad_x,
            layout.header.y + (header_h - Font_GetLineHeight(ui->font)) / 2},
      title, ui->font, th->text);

  header_sep =
      (rect){layout.frame.x, layout.frame.y + header_h, layout.frame.w, 1};
  Render_DrawRect(ctx, header_sep, Color_WithAlpha(th->border, 100));

  layout.footer = (rect){layout.frame.x,
                         layout.frame.y + layout.frame.h - footer_h,
                         layout.frame.w, footer_h};
  footer_sep = (rect){layout.footer.x, layout.footer.y, layout.footer.w, 1};
  Render_DrawRect(ctx, footer_sep, Color_WithAlpha(th->border, 80));

  layout.content = (rect){layout.frame.x + outer_pad_x,
                          layout.frame.y + header_h + body_pad_y,
                          layout.frame.w - (outer_pad_x * 2),
                          layout.frame.h - header_h - footer_h -
                              (body_pad_y * 2)};

  if (layout.content.w < 0) {
    layout.content.w = 0;
  }
  if (layout.content.h < 0) {
    layout.content.h = 0;
  }

  return layout;
}

void Dialog_EndShell(ui_context *ui, const dialog_shell_layout *layout) {
  (void)ui;
  (void)layout;
  UI_EndModal();
}

dialog_result Dialog_Render(ui_context *ui, rect bounds,
                            const dialog_config *config) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  dialog_result result = WB_DIALOG_RESULT_NONE;
  i32 outer_pad_x = th->spacing_xl;
  i32 body_pad_top = th->spacing_lg;
  i32 body_pad_bottom = th->spacing_xl;
  i32 footer_pad_y = th->spacing_lg;
  i32 actions_gap_x = th->spacing_md;
  i32 header_h = 52;
  i32 btn_w = 96;
  i32 btn_h = 36;
  i32 footer_h = btn_h + (footer_pad_y * 2);
  i32 body_content_h = 36;

  /* Dim background */
  color dim = Color_WithAlpha(th->background, 200);
  Render_DrawRect(ctx, bounds, dim);

  if (config->type == WB_DIALOG_TYPE_CONFIRM) {
    body_content_h = Dialog_GetConfirmBodyHeight(ui, config);
  }

  i32 dialog_w = DIALOG_WIDTH;
  i32 dialog_h =
      header_h + body_pad_top + body_content_h + body_pad_bottom + footer_h;

  rect dialog = {bounds.x + (bounds.w - dialog_w) / 2,
                 bounds.y + (bounds.h - dialog_h) / 2, dialog_w, dialog_h};

  /* Outer shadow/border */
  rect shadow = {dialog.x - 1, dialog.y - 1, dialog.w + 2, dialog.h + 2};
  Render_DrawRectRounded(ctx, shadow, th->radius_md + 1, th->border);
  UI_DrawPanel(dialog);

  /* Header */
  color title_color = config->is_danger ? th->error : th->text;
  rect header_rect = {dialog.x, dialog.y, dialog.w, header_h};
  v2i title_pos = {header_rect.x + outer_pad_x,
                   header_rect.y +
                       (header_h - Font_GetLineHeight(ui->font)) / 2};
  Render_DrawText(ctx, title_pos, config->title ? config->title : "Dialog",
                  ui->font, title_color);

  rect sep = {dialog.x, dialog.y + header_h, dialog.w, 1};
  Render_DrawRect(ctx, sep, Color_WithAlpha(th->border, 100));

  /* Content Area */
  rect body_rect = {dialog.x + outer_pad_x, dialog.y + header_h + body_pad_top,
                    dialog.w - (outer_pad_x * 2), body_content_h};

  if (config->type == WB_DIALOG_TYPE_CONFIRM) {
    Dialog_RenderConfirmBody(ui, body_rect, config);
  } else {
    Dialog_RenderInputBody(body_rect, config);
  }

  /* Footer Buttons */
  rect footer_rect = {dialog.x, dialog.y + dialog.h - footer_h, dialog.w,
                      footer_h};
  i32 btn_y = footer_rect.y + footer_pad_y;

  /* Cancel Button */
  rect confirm_rect = {dialog.x + dialog.w - outer_pad_x - btn_w, btn_y, btn_w,
                       btn_h};
  rect cancel_rect = {confirm_rect.x - actions_gap_x - btn_w, btn_y, btn_w,
                      btn_h};

  UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, cancel_rect);
  UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, btn_w);
  UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, btn_h);
  UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, Color_WithAlpha(th->panel_alt, 150));
  if (UI_Button(config->cancel_label ? config->cancel_label : "Cancel")) {
    result = WB_DIALOG_RESULT_CANCEL;
  }
  UI_PopStyle();
  UI_PopStyle();
  UI_PopStyle();
  UI_EndLayout();

  /* Confirm Button */
  UI_BeginLayout(WB_UI_LAYOUT_HORIZONTAL, confirm_rect);
  UI_PushStyleInt(WB_UI_STYLE_MIN_WIDTH, btn_w);
  UI_PushStyleInt(WB_UI_STYLE_MIN_HEIGHT, btn_h);
  if (config->is_danger) {
    UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, th->error);
    UI_PushStyleColor(WB_UI_STYLE_HOVER_COLOR, Color_Lighten(th->error, 0.1f));
  } else {
    UI_PushStyleColor(WB_UI_STYLE_BG_COLOR, th->accent);
  }

  const char *confirm_label = config->confirm_label;
  if (!confirm_label) {
    confirm_label = "Confirm";
  }

  if (UI_Button(confirm_label)) {
    result = WB_DIALOG_RESULT_CONFIRM;
  }
  UI_PopStyle();
  if (config->is_danger)
    UI_PopStyle();
  UI_PopStyle();
  UI_PopStyle();
  UI_EndLayout();

  return result;
}
