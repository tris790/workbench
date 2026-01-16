#ifndef WSH_ABBR_H
#define WSH_ABBR_H

typedef struct {
  char *key;
  char *value;
} wsh_abbr_t;

typedef struct {
  wsh_abbr_t *entries;
  int count;
  int capacity;
} wsh_abbr_map_t;

void WSH_Abbr_Init(wsh_abbr_map_t *map);
void WSH_Abbr_Free(wsh_abbr_map_t *map);
void WSH_Abbr_Add(wsh_abbr_map_t *map, const char *key, const char *value);
const char *WSH_Abbr_Expand(wsh_abbr_map_t *map, const char *key);

#endif
