/*
 * commands.h - Global Command Registration
 *
 * Defines functions to register application commands with the
 * command palette.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include "ui/components/command_palette.h"
#include "ui/layout.h"

/* Initialize the commands module with layout context */
void Commands_Init(layout_state *layout);

/* Register all built-in commands to the palette */
void Commands_Register(command_palette_state *palette);

/* Command implementations that might be called directly */
void Cmd_ViewToggleFullscreen(void *u);
void Cmd_ViewFocusNextPane(void *u);
void Cmd_ViewToggleSplit(void *u);

#endif /* COMMANDS_H */
