#include "config.h"
#include "../core/fs_watcher.h"
#include "../core/types.h"
#include "config_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_CONFIG_ENTRIES 256
#define MAX_DIAGNOSTICS 32

static ConfigEntry g_registry[MAX_CONFIG_ENTRIES];
static i32 g_registry_count = 0;

static char g_diagnostics[MAX_DIAGNOSTICS][512];
static i32 g_diagnostic_count = 0;

static fs_watcher g_config_watcher;

/* Forward declarations for parser functions (Task 09_03) */
extern b32 ConfigParser_Load(void);
extern b32 ConfigParser_Save(void);

extern const char *ConfigParser_GetPath(void);
extern u64 ConfigParser_GetModTime(void);

static u64 g_last_config_mtime = 0;

void Config_AddDiagnostic(const char *fmt, ...) {
  if (g_diagnostic_count < MAX_DIAGNOSTICS) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_diagnostics[g_diagnostic_count], sizeof(g_diagnostics[0]), fmt,
              args);
    va_end(args);
    g_diagnostic_count++;
  }
}

ConfigEntry *Config_GetEntryByKey(const char *key) {
  for (i32 i = 0; i < g_registry_count; ++i) {
    if (strcmp(g_registry[i].key, key) == 0) {
      return &g_registry[i];
    }
  }
  return NULL;
}

static ConfigEntry *GetOrCreateEntry(const char *key) {
  ConfigEntry *entry = Config_GetEntryByKey(key);
  if (entry)
    return entry;

  if (g_registry_count < MAX_CONFIG_ENTRIES) {
    entry = &g_registry[g_registry_count++];
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';
    return entry;
  }

  Config_AddDiagnostic("Config registry full, cannot add key: %s", key);
  return NULL;
}

static void SetDefaults(void) {
  Config_SetI64("config_version", 1);
  Config_SetI64("window.width", 1280);
  Config_SetI64("window.height", 720);
  Config_SetBool("window.maximized", (b32) false);
  Config_SetI64("ui.font_size", 16);
  Config_SetBool("ui.animations_enabled", (b32) true);
  Config_SetF64("ui.scroll_speed", 3.0);
  Config_SetBool("explorer.show_hidden", (b32) false);
  Config_SetBool("explorer.confirm_delete", (b32) true);
  Config_SetString("explorer.sort_type", "name");
  Config_SetString("explorer.sort_order", "ascending");
  Config_SetI64("terminal.font_size", 14);
  Config_SetI64("terminal.scrollback_lines", 10000);
}

/* Lifecycle */
b32 Config_Init(void) {
  g_registry_count = 0;
  g_diagnostic_count = 0;

  FSWatcher_Init(&g_config_watcher);
  const char *path = ConfigParser_GetPath();

  if (path && path[0]) {
    char dir_path[FS_MAX_PATH];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    // Simple directory extraction
    char *last_sep = strrchr(dir_path, '/');
#ifdef _WIN32
    char *win_sep = strrchr(dir_path, '\\');
    if (win_sep > last_sep)
      last_sep = win_sep;
#endif

    if (last_sep) {
      *last_sep = '\0';
      FSWatcher_WatchDirectory(&g_config_watcher, dir_path);
    }
  }

  SetDefaults();

  // ConfigParser_Load will overwrite defaults with values from file
  b32 result = ConfigParser_Load();
  if (result) {
    g_last_config_mtime = ConfigParser_GetModTime();
  }
  return result;
}

void Config_Shutdown(void) {
  FSWatcher_Shutdown(&g_config_watcher);
  // Currently using static storage, nothing to free specifically
  // but we could clear the registry
  g_registry_count = 0;
}

b32 Config_Poll(void) {
  if (FSWatcher_Poll(&g_config_watcher)) {
    u64 current_mtime = ConfigParser_GetModTime();
    if (current_mtime != g_last_config_mtime) {
      Config_Reload();
      return true;
    }
  }
  return false;
}

b32 Config_Reload(void) {
  g_registry_count = 0;
  g_diagnostic_count = 0;

  SetDefaults();
  b32 result = ConfigParser_Load();
  if (result) {
    g_last_config_mtime = ConfigParser_GetModTime();
  }
  return result;
}

b32 Config_Save(void) {
  b32 result = ConfigParser_Save();
  if (result) {
    g_last_config_mtime = ConfigParser_GetModTime();
  }
  return result;
}

/* Typed Getters */
b32 Config_GetBool(const char *key, b32 default_val) {
  ConfigEntry *entry = Config_GetEntryByKey(key);
  if (entry) {
    if (entry->type == CONFIG_TYPE_BOOL) {
      return entry->value.bool_val;
    } else {
      Config_AddDiagnostic("Type mismatch for key '%s': expected Bool", key);
    }
  }
  return default_val;
}

i64 Config_GetI64(const char *key, i64 default_val) {
  ConfigEntry *entry = Config_GetEntryByKey(key);
  if (entry) {
    if (entry->type == CONFIG_TYPE_I64) {
      return entry->value.i64_val;
    } else {
      Config_AddDiagnostic("Type mismatch for key '%s': expected I64", key);
    }
  }
  return default_val;
}

f64 Config_GetF64(const char *key, f64 default_val) {
  ConfigEntry *entry = Config_GetEntryByKey(key);
  if (entry) {
    if (entry->type == CONFIG_TYPE_F64) {
      return entry->value.f64_val;
    } else {
      Config_AddDiagnostic("Type mismatch for key '%s': expected F64", key);
    }
  }
  return default_val;
}

const char *Config_GetString(const char *key, const char *default_val) {
  ConfigEntry *entry = Config_GetEntryByKey(key);
  if (entry) {
    if (entry->type == CONFIG_TYPE_STRING) {
      return entry->value.string_val;
    } else {
      Config_AddDiagnostic("Type mismatch for key '%s': expected String", key);
    }
  }
  return default_val;
}

/* Typed Setters */
void Config_SetBool(const char *key, b32 value) {
  ConfigEntry *entry = GetOrCreateEntry(key);
  if (entry) {
    entry->type = CONFIG_TYPE_BOOL;
    entry->value.bool_val = value;
  }
}

void Config_SetI64(const char *key, i64 value) {
  ConfigEntry *entry = GetOrCreateEntry(key);
  if (entry) {
    entry->type = CONFIG_TYPE_I64;
    entry->value.i64_val = value;
  }
}

void Config_SetF64(const char *key, f64 value) {
  ConfigEntry *entry = GetOrCreateEntry(key);
  if (entry) {
    entry->type = CONFIG_TYPE_F64;
    entry->value.f64_val = value;
  }
}

void Config_SetString(const char *key, const char *value) {
  ConfigEntry *entry = GetOrCreateEntry(key);
  if (entry) {
    entry->type = CONFIG_TYPE_STRING;
    if (value) {
      strncpy(entry->value.string_val, value,
              sizeof(entry->value.string_val) - 1);
      entry->value.string_val[sizeof(entry->value.string_val) - 1] = '\0';
    } else {
      entry->value.string_val[0] = '\0';
    }
  }
}

/* Diagnostics */
b32 Config_HasErrors(void) { return g_diagnostic_count > 0; }

i32 Config_GetDiagnosticCount(void) { return g_diagnostic_count; }

const char *Config_GetDiagnosticMessage(i32 index) {
  if (index >= 0 && index < g_diagnostic_count) {
    return g_diagnostics[index];
  }
  return NULL;
}

/* Path */
const char *Config_GetPath(void) { return ConfigParser_GetPath(); }

/* Internal API for parser */
i32 Config_GetEntryCount(void) { return g_registry_count; }

ConfigEntry *Config_GetEntry(i32 index) {
  if (index >= 0 && index < g_registry_count) {
    return &g_registry[index];
  }
  return NULL;
}

const char *Config_GetEntryKey(i32 index) {
  ConfigEntry *entry = Config_GetEntry(index);
  return entry ? entry->key : NULL;
}

ConfigValueType Config_GetEntryType(i32 index) {
  ConfigEntry *entry = Config_GetEntry(index);
  return entry ? entry->type : CONFIG_TYPE_STRING;
}
