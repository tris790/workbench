/*
 * breadcrumb.c - Breadcrumb Navigation Component Implementation
 *
 * C99, handmade hero style.
 */

#include "breadcrumb.h"
#include "fs.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

void Breadcrumb_Render(ui_context *ui, rect bounds, const char *path) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;

  /* Draw background */
  Render_DrawRect(ctx, bounds, th->panel);

  /* Draw path */
  i32 padding = 8;
  v2i pos = {bounds.x + padding, bounds.y + (bounds.h - 16) / 2};

  /* Truncate path if too long */
  i32 max_width = bounds.w - padding * 2;
  i32 path_width = Font_MeasureWidth(ui->font, path);

  if (path_width > max_width) {
    /* Show "..." prefix */
    char display[FS_MAX_PATH];
    const char *start = path + strlen(path);

    while (start > path) {
      start--;
      if (*start == '/') {
        char temp[FS_MAX_PATH];
        snprintf(temp, sizeof(temp), "...%s", start);
        if (Font_MeasureWidth(ui->font, temp) <= max_width) {
          snprintf(display, sizeof(display), "...%s", start);
          Render_DrawText(ctx, pos, display, ui->font, th->text);
          goto done_breadcrumb;
        }
      }
    }
    /* Fallback - just show what fits */
    Render_DrawText(ctx, pos, "...", ui->font, th->text);
  } else {
    Render_DrawText(ctx, pos, path, ui->font, th->text);
  }

done_breadcrumb:;

  /* Bottom border */
  {
    rect border = {bounds.x, bounds.y + bounds.h - 1, bounds.w, 1};
    Render_DrawRect(ctx, border, th->border);
  }
}
