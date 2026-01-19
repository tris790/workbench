#include "../core/types.h"
#include "config_internal.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static char g_config_path[FS_MAX_PATH] = {0};

static char *TrimWhitespace(char *str) {
  char *end;
  while (isspace((unsigned char)*str))
    str++;
  if (*str == 0)
    return str;
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return str;
}

const char *ConfigParser_GetPath(void) {
  if (g_config_path[0] != '\0') {
    return g_config_path;
  }

#ifdef _WIN32
  const char *appdata = getenv("APPDATA");
  if (appdata) {
    char dir[FS_MAX_PATH];
    snprintf(dir, sizeof(dir), "%s\\workbench", appdata);
    CreateDirectoryA(dir, NULL);
    snprintf(g_config_path, sizeof(g_config_path), "%s\\workbench\\config",
             appdata);
  } else {
    strncpy(g_config_path, "config", sizeof(g_config_path) - 1);
  }
#else
  const char *home = getenv("HOME");
  if (home) {
    char dir[FS_MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/workbench", home);
    mkdir(dir, 0755);
    snprintf(g_config_path, sizeof(g_config_path),
             "%s/.config/workbench/config", home);
  } else {
    strncpy(g_config_path, "config", sizeof(g_config_path) - 1);
  }
#endif

  return g_config_path;
}

static b32 IsNumeric(const char *str) {
  if (*str == '-')
    str++;
  if (!*str)
    return false;
  while (*str) {
    if (!isdigit((unsigned char)*str) && *str != '.')
      return false;
    str++;
  }
  return true;
}

static b32 ContainsDot(const char *str) { return strchr(str, '.') != NULL; }

static b32 IsHex(const char *str) {
  return str[0] == '0' && (str[1] == 'x' || str[1] == 'X');
}

static const char *DEFAULT_CONFIG_CONTENT =
    "# Workbench Configuration\n"
    "# Lines starting with # are comments\n"
    "# Edit values and use \"Config: Reload\" command to apply\n"
    "\n"
    "config_version = 2\n"
    "\n"
    "# Window\n"
    "window.width = 1280\n"
    "window.height = 720\n"
    "window.maximized = false\n"
    "\n"
    "# UI\n"
    "ui.font_size = 16\n"
    "ui.animations_enabled = true\n"
    "ui.scroll_speed = 3.0\n"
    "\n"
    "# Explorer\n"
    "explorer.show_hidden = false\n"
    "explorer.confirm_delete = true\n"
    "\n"
    "# Terminal\n"
    "terminal.font_size = 14\n"
    "terminal.scrollback_lines = 10000\n"
    "terminal.shell = \"\"\n"
    "terminal.shell_mode = \"native\"\n"
    "\n"
    "# Theme overrides (hex colors use 0x prefix)\n"
    "# theme.accent_color = 0x4A9EFF\n";

b32 ConfigParser_Load(void) {
  const char *path = ConfigParser_GetPath();
  FILE *file = fopen(path, "r");
  if (!file) {
    if (errno == ENOENT) {
      // File not found, create default one
      file = fopen(path, "w");
      if (file) {
        fputs(DEFAULT_CONFIG_CONTENT, file);
        fclose(file);
        return true;
      }
    }
    Config_AddDiagnostic(
        "Failed to open config file for reading: %s (errno %d)", path, errno);
    return false;
  }

  char line[2048];
  i32 line_number = 0;
  while (fgets(line, sizeof(line), file)) {
    line_number++;
    char *trimmed = TrimWhitespace(line);

    if (trimmed[0] == '\0' || trimmed[0] == '#') {
      continue;
    }

    char *eq = strchr(trimmed, '=');
    if (!eq) {
      Config_AddDiagnostic("Parse error at line %d: Missing '='", line_number);
      continue;
    }

    *eq = '\0';
    char *key = TrimWhitespace(trimmed);
    char *val_str = TrimWhitespace(eq + 1);

    if (strlen(key) == 0) {
      Config_AddDiagnostic("Parse error at line %d: Empty key", line_number);
      continue;
    }

    ConfigEntry *existing = Config_GetEntryByKey(key);

    if (strcmp(val_str, "true") == 0 || strcmp(val_str, "false") == 0) {
      if (existing && existing->type != CONFIG_TYPE_BOOL) {
        Config_AddDiagnostic(
            "Type mismatch at line %d: key '%s' expected %s, got Bool",
            line_number, key,
            existing->type == CONFIG_TYPE_I64   ? "I64"
            : existing->type == CONFIG_TYPE_F64 ? "F64"
                                                : "String");
      } else {
        Config_SetBool(key, strcmp(val_str, "true") == 0);
      }
    } else if (IsHex(val_str)) {
      if (existing && existing->type != CONFIG_TYPE_I64) {
        Config_AddDiagnostic(
            "Type mismatch at line %d: key '%s' expected %s, got I64 (Hex)",
            line_number, key,
            existing->type == CONFIG_TYPE_BOOL  ? "Bool"
            : existing->type == CONFIG_TYPE_F64 ? "F64"
                                                : "String");
      } else {
        i64 hex_val = strtoll(val_str, NULL, 16);
        Config_SetI64(key, hex_val);
      }
    } else if (IsNumeric(val_str)) {
      if (ContainsDot(val_str)) {
        if (existing && existing->type != CONFIG_TYPE_F64) {
          Config_AddDiagnostic(
              "Type mismatch at line %d: key '%s' expected %s, got F64",
              line_number, key,
              existing->type == CONFIG_TYPE_BOOL  ? "Bool"
              : existing->type == CONFIG_TYPE_I64 ? "I64"
                                                  : "String");
        } else {
          f64 f_val = atof(val_str);
          Config_SetF64(key, f_val);
        }
      } else {
        if (existing && existing->type != CONFIG_TYPE_I64) {
          Config_AddDiagnostic(
              "Type mismatch at line %d: key '%s' expected %s, got I64",
              line_number, key,
              existing->type == CONFIG_TYPE_BOOL  ? "Bool"
              : existing->type == CONFIG_TYPE_F64 ? "F64"
                                                  : "String");
        } else {
          i64 i_val = atoll(val_str);
          Config_SetI64(key, i_val);
        }
      }
    } else {
      char *string_val = val_str;
      if (val_str[0] == '"') {
        size_t len = strlen(val_str);
        if (len >= 2 && val_str[len - 1] == '"') {
          val_str[len - 1] = '\0';
          string_val = val_str + 1;
        } else {
          Config_AddDiagnostic("Parse error at line %d: Unclosed quote",
                               line_number);
          string_val = val_str + 1;
        }
      }

      if (existing && existing->type != CONFIG_TYPE_STRING) {
        Config_AddDiagnostic(
            "Type mismatch at line %d: key '%s' expected %s, got String",
            line_number, key,
            existing->type == CONFIG_TYPE_BOOL  ? "Bool"
            : existing->type == CONFIG_TYPE_I64 ? "I64"
                                                : "F64");
      } else {
        Config_SetString(key, string_val);
      }
    }
  }

  fclose(file);
  return true;
}

b32 ConfigParser_Save(void) {
  const char *path = ConfigParser_GetPath();
  FILE *file = fopen(path, "w");
  if (!file) {
    Config_AddDiagnostic(
        "Failed to open config file for writing: %s (errno %d)", path, errno);
    return false;
  }

  fprintf(file, "# Workbench Configuration\n\n");

  i32 count = Config_GetEntryCount();
  for (i32 i = 0; i < count; ++i) {
    ConfigEntry *entry = Config_GetEntry(i);
    if (!entry)
      continue;

    fprintf(file, "%s = ", entry->key);
    switch (entry->type) {
    case CONFIG_TYPE_BOOL:
      fprintf(file, "%s", entry->value.bool_val ? "true" : "false");
      break;
    case CONFIG_TYPE_I64:
      // Check if it looks like a color (large positive hex-like value)
      // For now just output as decimal unless we want to be fancy.
      // The requirement says "hex color (parse as u32, store as i64)".
      // Let's see if we should always output i64 as decimal?
      // Maybe we can check if the key contains "color".
      if (strstr(entry->key, "color") || strstr(entry->key, "Color")) {
        fprintf(file, "0x%llX", (unsigned long long)entry->value.i64_val);
      } else {
        fprintf(file, "%lld", (long long)entry->value.i64_val);
      }
      break;
    case CONFIG_TYPE_F64:
      fprintf(file, "%.2f", entry->value.f64_val);
      break;
    case CONFIG_TYPE_STRING:
      fprintf(file, "\"%s\"", entry->value.string_val);
      break;
    }
    fprintf(file, "\n");
  }

  fclose(file);
  return true;
}

u64 ConfigParser_GetModTime(void) {
  const char *path = ConfigParser_GetPath();
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA attribs;
  if (GetFileAttributesExA(path, GetFileExInfoStandard, &attribs)) {
    return ((u64)attribs.ftLastWriteTime.dwHighDateTime << 32) |
           attribs.ftLastWriteTime.dwLowDateTime;
  }
#else
  struct stat st;
  if (stat(path, &st) == 0) {
#if defined(__APPLE__)
    return (u64)
        st.st_mtimespec.tv_sec; // nanoseconds would be st_mtimespec.tv_nsec
#else
    return (u64)st.st_mtime;
#endif
  }
#endif
  return 0;
}
