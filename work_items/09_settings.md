# Work Item 09: Persistent Configuration

## Objective

Implement a persistent configuration system that loads settings from disk at startup, supports runtime reload, and provides a central API for all configurable values.

## Requirements

- Human-readable/editable text file format
- Supports: bool, i64, f64, string, hex color (u32)
- Comments with `#`
- Cross-platform: Linux (`~/.config/workbench.txt`), Windows (`%APPDATA%\workbench.txt`)
- Paths in config always use forward slashes `/` (normalized internally)
- Invalid config doesn't crash—errors shown via diagnostics modal
- No restart required to apply changes
- Central `Config_*` API—single source of truth

---

## File Format: `workbench.txt`

Simple key=value format. Order-independent.

```ini
# Workbench Configuration
# Lines starting with # are comments

config_version = 1

# Window
window.width = 1280
window.height = 720
window.maximized = false

# UI
ui.font_size = 16
ui.animations_enabled = true
ui.scroll_speed = 3.0

# Explorer
explorer.show_hidden = false
explorer.confirm_delete = true

# Theme overrides (future)
theme.accent_color = 0x4A9EFF

# Terminal
terminal.font_size = 14
terminal.scrollback_lines = 10000

# String values use double quotes
# example.path = "/home/user/documents"
# example.name = "My Workbench"
```

### Syntax Rules

| Element | Syntax | Example |
|---------|--------|---------|
| Comment | `# ...` | `# This is a comment` |
| Bool | `true` / `false` | `enabled = true` |
| Integer | Digits | `width = 1280` |
| Float | Digits with `.` | `speed = 3.0` |
| Hex color | `0x` prefix | `color = 0x4A9EFF` |
| String | Double-quoted | `name = "value"` |

### Parsing Rules

1. Empty lines and lines starting with `#` are ignored
2. Lines must contain `=` to be valid
3. Whitespace around `=` is trimmed
4. Unknown keys are silently ignored (forward compatibility)
5. Type mismatches: use default, add diagnostic
6. Paths in values always use `/` regardless of OS

---

## Config File Locations

| Platform | Path |
|----------|------|
| Linux | `~/.config/workbench.txt` |
| Windows | `%APPDATA%\workbench.txt` |

---

## Architecture

```
src/config/
├── config.h           # Public API
├── config.c           # Registry, getters/setters
└── config_parser.c    # File I/O and parsing
```

### API Summary

```c
/* Lifecycle */
b32 Config_Init(void);        /* Load from disk */
void Config_Shutdown(void);
b32 Config_Reload(void);      /* Re-read file */
b32 Config_Save(void);        /* Write to disk */

/* Typed Getters (return default if not set) */
b32 Config_GetBool(const char *key, b32 default_val);
i64 Config_GetI64(const char *key, i64 default_val);
f64 Config_GetF64(const char *key, f64 default_val);
const char *Config_GetString(const char *key, const char *default_val);

/* Typed Setters (in-memory until Config_Save) */
void Config_SetBool(const char *key, b32 value);
void Config_SetI64(const char *key, i64 value);
void Config_SetF64(const char *key, f64 value);
void Config_SetString(const char *key, const char *value);

/* Diagnostics */
b32 Config_HasErrors(void);
i32 Config_GetDiagnosticCount(void);
const char *Config_GetDiagnosticMessage(i32 index);

/* Path */
const char *Config_GetPath(void);
```

---

## Initial Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `config_version` | i64 | 1 | For future migrations |
| `window.width` | i64 | 1280 | Initial window width |
| `window.height` | i64 | 720 | Initial window height |
| `window.maximized` | bool | false | Start maximized |
| `ui.font_size` | i64 | 16 | Main UI font size |
| `ui.animations_enabled` | bool | true | Enable animations |
| `ui.scroll_speed` | f64 | 3.0 | Scroll multiplier |
| `explorer.show_hidden` | bool | false | Show hidden files |
| `explorer.confirm_delete` | bool | true | Require delete confirmation |
| `terminal.font_size` | i64 | 14 | Terminal font size |
| `terminal.scrollback` | i64 | 10000 | Scrollback buffer lines |

---

## Command Palette Integration

| Command | Action |
|---------|--------|
| `Config: Reload` | Re-read config file |
| `Config: Show Diagnostics` | Open modal with loaded values and errors |
| `Config: Open File` | Open config in default editor |

---

## Error Handling

1. **File not found**: Create with defaults on first run
2. **Parse error**: Add to diagnostics, use defaults
3. **Type mismatch**: Add to diagnostics, use default
4. **Unknown key**: Ignore silently

Diagnostics modal shows:
- Config file path
- All loaded values with types
- Any errors with line numbers

---

## Deliverables

1. `src/config/config.h` - Public API header
2. `src/config/config.c` - Registry and getters/setters
3. `src/config/config_parser.c` - File reading/writing/parsing
4. Update `main.c` - Call `Config_Init()` at startup
5. Update `commands.c` - Add config commands
6. Update `build.sh` - Include new source files

---

## Success Criteria

- [ ] Config file created on first run with defaults
- [ ] All settings load correctly at startup
- [ ] `Config: Reload` command works
- [ ] Invalid config shows errors in diagnostics modal
- [ ] Works on both Linux and Windows
- [ ] No crashes on malformed config

## Dependencies

- 08_context_menu.md

## Next Work Item

10_polish_and_integration.md

## Future Scope (Not in this work item)

- Theme customization and theme files
- Keyboard shortcut customization
- Custom shell commands
- Settings UI panel
