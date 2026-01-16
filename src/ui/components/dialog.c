/*
 * dialog.c - Reusable Dialog Component Implementation
 *
 * C99, handmade hero style.
 */

#include "dialog.h"
#include "theme.h"

dialog_result Dialog_Render(ui_context *ui, rect bounds,
                            const dialog_config *config) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  dialog_result result = DIALOG_RESULT_NONE;

  /* Dim background */
  color dim = Color_WithAlpha(th->background, 200);
  Render_DrawRect(ctx, bounds, dim);

  /* Calculate dialog height based on content */
  i32 content_base_h = 140; /* Header + Footer + padding */
  i32 text_h = 0;

  if (config->type == DIALOG_TYPE_CONFIRM) {
    if (config->message.lines) {
      text_h = Text_GetWrappedHeight(config->message.count, ui->main_font) + 20;
    } else {
      text_h = 40;
    }
    if (text_h < 60)
      text_h = 60;
  } else {
    text_h = 60; /* Fixed height for input dialogs */
  }

  i32 dialog_w = DIALOG_WIDTH;
  i32 dialog_h = content_base_h + text_h;

  rect dialog = {bounds.x + (bounds.w - dialog_w) / 2,
                 bounds.y + (bounds.h - dialog_h) / 2, dialog_w, dialog_h};

  /* Outer shadow/border */
  rect shadow = {dialog.x - 1, dialog.y - 1, dialog.w + 2, dialog.h + 2};
  Render_DrawRectRounded(ctx, shadow, th->radius_md + 1, th->border);
  UI_DrawPanel(dialog);

  /* Header */
  color title_color = config->is_danger ? th->error : th->text;
  i32 header_h = 44;
  rect header_rect = {dialog.x, dialog.y, dialog.w, header_h};
  v2i title_pos = {header_rect.x + th->spacing_lg,
                   header_rect.y +
                       (header_h - Font_GetLineHeight(ui->main_font)) / 2};
  Render_DrawText(ctx, title_pos, config->title ? config->title : "Dialog",
                  ui->main_font, title_color);

  rect sep = {dialog.x, dialog.y + header_h, dialog.w, 1};
  Render_DrawRect(ctx, sep, Color_WithAlpha(th->border, 100));

  /* Content Area */
  i32 content_y = dialog.y + header_h + th->spacing_lg;
  i32 content_w = dialog.w - (th->spacing_lg * 2);

  if (config->type == DIALOG_TYPE_CONFIRM) {
    /* Warning Icon */
    i32 icon_size = 20;
    rect icon_rect = {dialog.x + th->spacing_lg, content_y, icon_size,
                      icon_size};
    Render_DrawRectRounded(ctx, icon_rect, 4.0f, th->error);

    i32 text_x = icon_rect.x + icon_size + th->spacing_md;
    v2i msg_pos = {text_x,
                   content_y + (icon_size - Font_GetLineHeight(ui->main_font)) / 2};

    /* Render wrapped text */
    if (config->message.lines) {
      for (i32 i = 0; i < config->message.count; i++) {
        Render_DrawText(ctx, msg_pos, config->message.lines[i], ui->main_font,
                        th->text);
        msg_pos.y += Font_GetLineHeight(ui->main_font);
      }
    } else {
      Render_DrawText(ctx, msg_pos, "Are you sure?", ui->main_font, th->text);
      msg_pos.y += Font_GetLineHeight(ui->main_font);
    }

    if (config->hint) {
      v2i hint_pos = {text_x, msg_pos.y + 12};
      Render_DrawText(ctx, hint_pos, config->hint, ui->main_font, th->text_muted);
    }
  } else {
    /* Text input area */
    rect input_rect = {dialog.x + th->spacing_lg, content_y, content_w, 36};
    UI_PushStyleInt(UI_STYLE_PADDING, 8);
    UI_BeginLayout(UI_LAYOUT_VERTICAL, input_rect);
    if (config->input_buffer && config->input_state) {
      UI_TextInput(config->input_buffer, config->input_buffer_size,
                   config->placeholder ? config->placeholder : "Enter text...",
                   config->input_state);
    }
    UI_EndLayout();
    UI_PopStyle();
  }

  /* Footer Buttons */
  i32 footer_h = 50;
  rect footer_rect = {dialog.x, dialog.y + dialog.h - footer_h, dialog.w,
                      footer_h};

  i32 btn_w = 90;
  i32 btn_h = 30;
  i32 btn_y = footer_rect.y + (footer_h - btn_h) / 2;

  /* Cancel Button */
  rect cancel_rect = {footer_rect.x + footer_rect.w - (btn_w * 2) -
                          (th->spacing_lg * 2),
                      btn_y, btn_w, btn_h};
  UI_BeginLayout(UI_LAYOUT_HORIZONTAL, cancel_rect);
  UI_PushStyleColor(UI_STYLE_BG_COLOR, Color_WithAlpha(th->panel_alt, 150));
  if (UI_Button(config->cancel_label ? config->cancel_label : "Cancel")) {
    result = DIALOG_RESULT_CANCEL;
  }
  UI_PopStyle();
  UI_EndLayout();

  /* Confirm Button */
  rect confirm_rect = {footer_rect.x + footer_rect.w - btn_w - th->spacing_lg,
                       btn_y, btn_w, btn_h};
  UI_BeginLayout(UI_LAYOUT_HORIZONTAL, confirm_rect);
  if (config->is_danger) {
    UI_PushStyleColor(UI_STYLE_BG_COLOR, th->error);
    UI_PushStyleColor(UI_STYLE_HOVER_COLOR, Color_Lighten(th->error, 0.1f));
  } else {
    UI_PushStyleColor(UI_STYLE_BG_COLOR, th->accent);
  }

  const char *confirm_label = config->confirm_label;
  if (!confirm_label) {
    confirm_label = "Confirm";
  }

  if (UI_Button(confirm_label)) {
    result = DIALOG_RESULT_CONFIRM;
  }
  UI_PopStyle();
  if (config->is_danger)
    UI_PopStyle();
  UI_EndLayout();

  return result;
}
