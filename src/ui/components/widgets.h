#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "../ui.h"

// Basic widgets
b32 UI_Button(const char *label);
void UI_Label(const char *text);
b32 UI_Selectable(const char *label, b32 selected);
void UI_Separator(void);
void UI_DrawPanel(rect bounds);

#endif
