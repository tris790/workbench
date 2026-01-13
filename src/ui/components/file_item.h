/*
 * file_item.h - File List Item Component
 *
 * Renders a single file/folder entry in a list view.
 * C99, handmade hero style.
 */

#ifndef FILE_ITEM_H
#define FILE_ITEM_H

#include "../../core/fs.h"
#include "../ui.h"

/* Configuration for file item rendering */
typedef struct {
  i32 icon_size;
  i32 icon_padding;
  b32 show_size; /* Show file size column */
} file_item_config;

/* Default configuration */
#define FILE_ITEM_CONFIG_DEFAULT                                               \
  (file_item_config) { .icon_size = 16, .icon_padding = 6, .show_size = true }

/* Render a file item row.
 *
 * @param ui UI context for rendering.
 * @param entry The file system entry to render.
 * @param bounds Rectangle to draw the item in.
 * @param is_selected Whether this item is currently selected.
 * @param is_hovered Whether the mouse is over this item.
 * @param config Rendering configuration.
 */
void FileItem_Render(ui_context *ui, fs_entry *entry, rect bounds,
                     b32 is_selected, b32 is_hovered,
                     const file_item_config *config);

#endif /* FILE_ITEM_H */
