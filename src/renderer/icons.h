/*
 * icons.h - Simple Geometric Icons for Workbench
 *
 * Renders file type icons using basic shapes.
 * C99, handmade hero style.
 */

#ifndef ICONS_H
#define ICONS_H

#include "fs.h"
#include "renderer.h"
#include "theme.h"

/* Draw an icon for the given file type */
void Icon_Draw(render_context *ctx, rect bounds, file_icon_type type, color c);

/* Get primary color for file icon type (for additional styling) */
color Icon_GetTypeColor(file_icon_type type, const theme *t);

#endif /* ICONS_H */
