# Work Item 13: Unified Text Input

## Objective
Unify text input behavior across all UI components so they share the same editing capabilities while preserving their distinct visual designs.

## Background
Currently, text input components have inconsistent features:
- **Command Palette** and **Dialog**: Full editing (selection, copy/paste, undo, word navigation)
- **Quick Filter**: Simple append-only (no selection, no copy/paste, no word navigation)

## Requirements
- All text input components use `UI_ProcessTextInput()` as the shared text processing engine
- All components support: selection (Shift+Arrow), Ctrl+A, Ctrl+C/X/V, Ctrl+Z, Ctrl+Arrow word jump, Home/End
- Visual rendering remains component-specific (no visual changes)
- Key repeat works consistently for held keys

## Deliverables

### 1. Quick Filter Refactoring (`quick_filter.c`)
- Replace custom character handling with `UI_ProcessTextInput()` call
- Add activation logic on first printable character
- Add deactivation logic when buffer becomes empty
- Keep existing visual rendering unchanged

### 2. UI Text Processing Enhancement (`ui.c`)
- Add `Input_KeyRepeat()` support for arrow keys (continuous cursor movement when held)

## Success Criteria
- All text input fields support same keyboard shortcuts
- Quick Filter supports selection, copy/paste, undo, word navigation
- No visual changes to any component
- Backspace clears Quick Filter and hides it when empty
- ESC still clears Quick Filter

## Dependencies
- 02_ui_framework.md (UI_ProcessTextInput foundation)
- 05_command_palette.md (reference implementation)

## Notes
- Terminal is intentionally excluded - it forwards keystrokes to PTY process
