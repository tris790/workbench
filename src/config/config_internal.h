#ifndef CONFIG_INTERNAL_H
#define CONFIG_INTERNAL_H

#include "config.h"

typedef struct {
  char key[256];
  ConfigValueType type;
  union {
    b32 bool_val;
    i64 i64_val;
    f64 f64_val;
    char string_val[1024];
  } value;
} ConfigEntry;

// Internal API for the parser
i32 Config_GetEntryCount(void);
ConfigEntry *Config_GetEntry(i32 index);
ConfigEntry *Config_GetEntryByKey(const char *key);
void Config_AddDiagnostic(const char *fmt, ...);

#endif /* CONFIG_INTERNAL_H */
