/*
 * breadcrumb.h - Breadcrumb Navigation Component
 *
 * Displays and truncates file paths with ellipsis prefix.
 * C99, handmade hero style.
 */

#ifndef BREADCRUMB_H
#define BREADCRUMB_H

#include "ui.h"

/* Render a breadcrumb path bar.
 * Automatically truncates long paths with "..." prefix.
 *
 * @param ui UI context for rendering.
 * @param bounds Rectangle to draw the breadcrumb in.
 * @param path The full path string to display.
 */
void Breadcrumb_Render(ui_context *ui, rect bounds, const char *path);

#endif /* BREADCRUMB_H */
