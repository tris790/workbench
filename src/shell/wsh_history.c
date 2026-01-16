#define _POSIX_C_SOURCE 200809L
#include "wsh_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define WSH_HISTORY_MAX_ENTRIES 1000

static char *get_history_path() {
  const char *home = getenv("HOME");
  if (!home)
    return NULL;

  static char path[4096];
  snprintf(path, sizeof(path), "%s/.local/share/wsh", home);

  struct stat st = {0};
  if (stat(path, &st) == -1) {
#ifdef _WIN32
    mkdir(path);
#else
    mkdir(path, 0700);
#endif
  }

  strcat(path, "/wsh_history");
  return path;
}

void WSH_History_Init(wsh_history_t *hist) {
  hist->count = 0;
  hist->capacity = 128; // Start small
  hist->entries = malloc(sizeof(wsh_history_entry_t) * hist->capacity);
}

void WSH_History_Destroy(wsh_history_t *hist) {
  for (u32 i = 0; i < hist->count; i++) {
    free(hist->entries[i].cmd);
  }
  free(hist->entries);
  hist->entries = NULL;
  hist->count = 0;
}

static void append_to_file(const char *cmd, u64 when) {
  char *path = get_history_path();
  if (!path)
    return;

  FILE *f = fopen(path, "a");
  if (!f)
    return;

  // lock? For MVP maybe skip flock
  fprintf(f, "- cmd: %s\n", cmd);
  fprintf(f, "  when: %llu\n", (unsigned long long)when);
  fclose(f);
}

void WSH_History_Add(wsh_history_t *hist, const char *cmd) {
  if (!cmd || strlen(cmd) == 0)
    return;

  // De-duplication: check last entry
  if (hist->count > 0) {
    if (strcmp(hist->entries[hist->count - 1].cmd, cmd) == 0) {
      return;
    }
  }

  if (hist->count >= WSH_HISTORY_MAX_ENTRIES) {
    /* Capacity is already at MAX, so we must shift or rotate.
       For simple array, we shift left by 1. */
    free(hist->entries[0].cmd);
    memmove(&hist->entries[0], &hist->entries[1],
            sizeof(wsh_history_entry_t) * (hist->count - 1));
    hist->count--;
  }

  if (hist->count >= hist->capacity) {
    hist->capacity *= 2;
    hist->entries =
        realloc(hist->entries, sizeof(wsh_history_entry_t) * hist->capacity);
  }

  wsh_history_entry_t *entry = &hist->entries[hist->count++];
  entry->cmd = strdup(cmd);
  entry->timestamp = (u64)time(NULL);

  append_to_file(cmd, entry->timestamp);
}

void WSH_History_Load(wsh_history_t *hist) {
  char *path = get_history_path();
  if (!path)
    return;

  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char line[4096];
  char *current_cmd = NULL;
  u64 current_when = 0;

  while (fgets(line, sizeof(line), f)) {
    // Trim newline
    line[strcspn(line, "\n")] = 0;

    if (strncmp(line, "- cmd: ", 7) == 0) {
      // If we have a pending command (from previous iteration), add it
      if (current_cmd) {
        // Internal add without appending to file
        if (hist->count >= WSH_HISTORY_MAX_ENTRIES) {
          free(hist->entries[0].cmd);
          memmove(&hist->entries[0], &hist->entries[1],
                  sizeof(wsh_history_entry_t) * (hist->count - 1));
          hist->count--;
        }
        if (hist->count >= hist->capacity) {
          hist->capacity *= 2;
          hist->entries = realloc(hist->entries,
                                  sizeof(wsh_history_entry_t) * hist->capacity);
        }
        hist->entries[hist->count].cmd = current_cmd;
        hist->entries[hist->count].timestamp = current_when;
        hist->count++;
        current_cmd = NULL;
      }
      current_cmd = strdup(line + 7);
    } else if (strncmp(line, "  when: ", 8) == 0) {
      current_when = strtoull(line + 8, NULL, 10);
    }
  }

  // Add last one
  if (current_cmd) {
    if (hist->count >= WSH_HISTORY_MAX_ENTRIES) {
      free(hist->entries[0].cmd);
      memmove(&hist->entries[0], &hist->entries[1],
              sizeof(wsh_history_entry_t) * (hist->count - 1));
      hist->count--;
    }
    if (hist->count >= hist->capacity) {
      hist->capacity *= 2;
      hist->entries =
          realloc(hist->entries, sizeof(wsh_history_entry_t) * hist->capacity);
    }
    hist->entries[hist->count].cmd = current_cmd;
    hist->entries[hist->count].timestamp = current_when;
    hist->count++;
  }

  fclose(f);
}

const char *WSH_History_GetSuggestion(wsh_history_t *hist, const char *prefix) {
  if (!prefix || strlen(prefix) == 0)
    return NULL;

  size_t len = strlen(prefix);

  // Iterate backwards
  for (int i = hist->count - 1; i >= 0; i--) {
    if (strncmp(hist->entries[i].cmd, prefix, len) == 0) {
      // Avoid suggesting the exact same thing if it matches perfectly
      // (optional, but usually we want to see beyond the prefix)
      if (strcmp(hist->entries[i].cmd, prefix) == 0)
        continue;

      return hist->entries[i].cmd;
    }
  }
  return NULL;
}
