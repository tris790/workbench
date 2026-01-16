#include "wsh_abbr.h"
#include <stdlib.h>
#include <string.h>

void WSH_Abbr_Init(wsh_abbr_map_t *map) {
  memset(map, 0, sizeof(wsh_abbr_map_t));
}

void WSH_Abbr_Free(wsh_abbr_map_t *map) {
  for (int i = 0; i < map->count; i++) {
    free(map->entries[i].key);
    free(map->entries[i].value);
  }
  free(map->entries);
  map->count = 0;
  map->capacity = 0;
}

void WSH_Abbr_Add(wsh_abbr_map_t *map, const char *key, const char *value) {
  /* Check if exists */
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->entries[i].key, key) == 0) {
      free(map->entries[i].value);
      map->entries[i].value = strdup(value);
      return;
    }
  }

  if (map->count >= map->capacity) {
    int new_cap = map->capacity == 0 ? 16 : map->capacity * 2;
    map->entries = realloc(map->entries, new_cap * sizeof(wsh_abbr_t));
    map->capacity = new_cap;
  }

  map->entries[map->count].key = strdup(key);
  map->entries[map->count].value = strdup(value);
  map->count++;
}

const char *WSH_Abbr_Expand(wsh_abbr_map_t *map, const char *key) {
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->entries[i].key, key) == 0) {
      return map->entries[i].value;
    }
  }
  return NULL;
}
