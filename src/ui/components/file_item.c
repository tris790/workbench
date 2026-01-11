/*
 * file_item.c - File List Item Component Implementation
 *
 * C99, handmade hero style.
 */

#include "file_item.h"
#include "icons.h"
#include "theme.h"

#include <string.h>

void FileItem_Render(ui_context *ui, fs_entry *entry, rect bounds,
                     b32 is_selected, b32 is_hovered,
                     const file_item_config *config) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;

  /* Background */
  if (is_selected) {
    Render_DrawRectRounded(ctx, bounds, 4.0f, th->accent);
  } else if (is_hovered) {
    Render_DrawRectRounded(ctx, bounds, 4.0f, th->panel);
  }

  i32 x = bounds.x + config->icon_padding;
  i32 y = bounds.y;

  /* Icon */
  color icon_color =
      is_selected ? th->background : Icon_GetTypeColor(entry->icon, th);
  rect icon_bounds = {x, y + (bounds.h - config->icon_size) / 2,
                      config->icon_size, config->icon_size};
  Icon_Draw(ctx, icon_bounds, entry->icon, icon_color);

  x += config->icon_size + config->icon_padding;

  /* Filename */
  color text_color = is_selected ? th->background : th->text;
  v2i text_pos = {x, y + (bounds.h - Font_GetLineHeight(ui->font)) / 2};

  /* Check if file is hidden */
  b32 is_hidden = entry->name[0] == '.' && strcmp(entry->name, "..") != 0;
  if (is_hidden && !is_selected) {
    text_color = th->text_muted;
  }

  Render_DrawText(ctx, text_pos, entry->name, ui->font, text_color);

  /* Size column */
  if (config->show_size && !entry->is_directory) {
    char size_str[32];
    FS_FormatSize(entry->size, size_str, sizeof(size_str));

    i32 size_width = Font_MeasureWidth(ui->font, size_str);
    v2i size_pos = {bounds.x + bounds.w - size_width - 8,
                    y + (bounds.h - Font_GetLineHeight(ui->font)) / 2};
    Render_DrawText(ctx, size_pos, size_str, ui->font,
                    is_selected ? th->background : th->text_muted);
  }
}
