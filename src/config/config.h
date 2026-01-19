#ifndef CONFIG_H
#define CONFIG_H

#include "../core/types.h"

/* Lifecycle */
b32 Config_Init(void); /* Load from disk */
void Config_Shutdown(void);
b32 Config_Reload(void); /* Re-read file */
b32 Config_Save(void);   /* Write to disk */
b32 Config_NeedsUpgrade(void);
b32 Config_Upgrade(void);

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
b32 Config_Poll(void); /* Returns true if config was reloaded */

#endif /* CONFIG_H */
