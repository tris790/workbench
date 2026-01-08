# Work Item 09: Settings System

## Objective
Implement a settings system for customizing the application behavior and appearance.

## Requirements
- Persistent settings stored in config file
- Settings UI accessible via command palette
- Theme customization
- Keyboard shortcut customization

## Deliverables

### 1. Settings Core (`settings.h` / `settings.c`)
```c
typedef struct {
    // Appearance
    String theme_name;
    String font_family;
    f32 font_size;
    bool show_hidden_files;
    bool show_file_size;
    bool show_modified_time;
    
    // Behavior
    bool auto_preview;
    bool confirm_delete;
    bool terminal_sync_cwd;
    LayoutMode default_layout;
    
    // Keyboard shortcuts
    KeyBinding *bindings;
    u32 binding_count;
    
    // Custom commands
    CustomCommand *custom_commands;
    u32 custom_command_count;
} Settings;
```

### 2. Config File (`config.h` / `config.c`)
- JSON or simple key=value format
- Located in ~/.config/wb/config.json (XDG compliant)
- Auto-create with defaults on first run
- Reload on change (optional: file watcher)

### 3. Settings UI
- Accessible via `> Settings` command
- Categorized sections
- Live preview for visual changes
- Apply/Cancel/Reset to defaults

### 4. Theme Management
- Multiple built-in themes
- Custom theme support
- Theme files in ~/.config/wb/themes/
- Live theme switching

### 5. Keyboard Shortcuts
- View all shortcuts
- Customize any shortcut
- Conflict detection
- Reset to defaults

### 6. Custom Commands (Extensibility)
```json
{
  "custom_commands": [
    {
      "name": "Open in VSCode",
      "command": "code ${file}",
      "context": "file"
    }
  ]
}
```

## Success Criteria
- Settings persist across restarts
- All settings work correctly
- Theme changes apply immediately
- Custom commands execute properly
- No crashes on invalid config

## Dependencies
- 08_context_menu.md

## Next Work Item
10_polish_and_integration.md
