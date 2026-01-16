#define _POSIX_C_SOURCE 200809L
#include "wsh_completion.h"
#include "wsh_pal.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void WSH_Pager_Init(wsh_pager_t *pager) {
  memset(pager, 0, sizeof(wsh_pager_t));
  pager->selected_index = 0;
  pager->active = false;
}

void WSH_Pager_Free(wsh_pager_t *pager) {
  WSH_Pager_Clear(pager);
  free(pager->candidates);
  pager->candidates = NULL;
  pager->capacity = 0;
  if (pager->filter) {
    free(pager->filter);
    pager->filter = NULL;
  }
}

void WSH_Pager_Clear(wsh_pager_t *pager) {
  for (int i = 0; i < pager->count; i++) {
    free(pager->candidates[i].display);
    free(pager->candidates[i].value);
    if (pager->candidates[i].description) {
      free(pager->candidates[i].description);
    }
  }
  pager->count = 0;
  pager->selected_index = 0;
  /* Keep capacity to avoid realloc churn */
}

void WSH_Pager_Add(wsh_pager_t *pager, const char *display, const char *value,
                   const char *desc) {
  if (pager->count >= pager->capacity) {
    int new_cap = pager->capacity == 0 ? 16 : pager->capacity * 2;
    pager->candidates =
        realloc(pager->candidates, new_cap * sizeof(wsh_completion_t));
    pager->capacity = new_cap;
  }

  wsh_completion_t *comp = &pager->candidates[pager->count++];
  comp->display = strdup(display);
  comp->value = strdup(value ? value : display);
  comp->description = desc ? strdup(desc) : NULL;
}

static void complete_paths(wsh_pager_t *pager, const char *prefix,
                           const char *cwd) {
  char search_path[4096];
  char file_prefix[256];

  /* Determine directory to search and prefix */
  const char *last_slash = strrchr(prefix, '/');
  if (last_slash) {
    int dir_len = last_slash - prefix + 1;
    strncpy(search_path, prefix, dir_len);
    search_path[dir_len] = '\0';
    strcpy(file_prefix, last_slash + 1);
  } else {
    strcpy(search_path, "./");
    strcpy(file_prefix, prefix);
  }

  DIR *d = opendir(search_path);
  if (!d)
    return;

  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
      continue;

    if (strncmp(dir->d_name, file_prefix, strlen(file_prefix)) == 0) {
      char full_val[8192];
      /* Construct full value to replace with */
      if (last_slash) {
        snprintf(full_val, sizeof(full_val), "%s%s", search_path, dir->d_name);
      } else {
        strcpy(full_val, dir->d_name);
      }

      /* Check if directory to append slash */
      struct stat st;
      char full_path[8192];
      if (search_path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", search_path,
                 dir->d_name);
      } else {
        snprintf(full_path, sizeof(full_path), "%s/%s%s", cwd, search_path,
                 dir->d_name);
      }

      if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strcat(full_val, "/");
      }

      WSH_Pager_Add(pager, dir->d_name, full_val, "File");
    }
  }
  closedir(d);
}

static void complete_commands(wsh_pager_t *pager, const char *prefix) {
  char *path_env = getenv("PATH");
  if (!path_env)
    return;

  char *path_dup = strdup(path_env);
  const char *sep = WSH_PAL_GetPathSeparator();
  char *dir_path = strtok(path_dup, sep);

  while (dir_path) {
    DIR *d = opendir(dir_path);
    if (d) {
      struct dirent *dir;
      while ((dir = readdir(d)) != NULL) {
        if (strncmp(dir->d_name, prefix, strlen(prefix)) == 0) {
          WSH_Pager_Add(pager, dir->d_name, dir->d_name, "Command");
        }
      }
      closedir(d);
    }
    dir_path = strtok(NULL, sep);
  }
  free(path_dup);
}

static void scan_man_page(const char *cmd, const char *cache_path) {
  char cmd_line[1024];

  /* Create dir */
  char dir[1024];
  strncpy(dir, cache_path, sizeof(dir));
  char *slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    WSH_PAL_MkdirRecursive(dir);
  }

  snprintf(cmd_line, sizeof(cmd_line), "man %s 2>/dev/null | col -b", cmd);
  FILE *in = popen(cmd_line, "r");
  if (!in)
    return;

  FILE *out = fopen(cache_path, "w");
  if (!out) {
    pclose(in);
    return;
  }

  char line[4096];
  while (fgets(line, sizeof(line), in)) {
    char *p = line;
    while (isspace(*p))
      p++;
    if (*p == '-') {
      char *desc_start = strstr(p, "  ");
      char flags[256];
      char desc[256] = "Flag";

      if (desc_start) {
        int len = desc_start - p;
        if (len >= 255)
          len = 255;
        strncpy(flags, p, len);
        flags[len] = '\0';

        char *d = desc_start;
        while (isspace(*d))
          d++;
        strncpy(desc, d, 255);
        desc[255] = '\0';
        desc[strcspn(desc, "\n")] = 0;
      } else {
        strncpy(flags, p, 255);
        flags[strcspn(flags, "\n")] = 0;
      }

      fprintf(out, "%s|%s\n", flags, desc);
    }
  }

  fclose(out);
  pclose(in);
}

static void complete_flags(wsh_pager_t *pager, const char *cmd,
                           const char *prefix) {
  if (prefix[0] != '-')
    return;

  char cache_path[1024];
  char *home = getenv("HOME");
  if (!home)
    return;
  snprintf(cache_path, sizeof(cache_path),
           "%s/.local/state/wsh/completions/%s.comp", home, cmd);

  if (access(cache_path, F_OK) != 0) {
    /* Generate cache */
    scan_man_page(cmd, cache_path);
  }

  FILE *f = fopen(cache_path, "r");
  if (!f)
    return;

  char line[4096];
  while (fgets(line, sizeof(line), f)) {
    char *pipe = strchr(line, '|');
    if (pipe) {
      *pipe = '\0';
      char *flags = line;
      char *desc = pipe + 1;
      desc[strcspn(desc, "\n")] = 0;

      char *dup = strdup(flags);
      char *tok = strtok(dup, ", ");
      while (tok) {
        if (tok[0] == '-') {
          if (strncmp(tok, prefix, strlen(prefix)) == 0) {
            WSH_Pager_Add(pager, tok, tok, desc);
          }
        }
        tok = strtok(NULL, ", ");
      }
      free(dup);
    }
  }
  fclose(f);
}

void WSH_Complete(wsh_pager_t *pager, const char *line, int cursor_pos,
                  const char *cwd) {
  WSH_Pager_Clear(pager);

  int start = cursor_pos;
  while (start > 0 && !isspace(line[start - 1])) {
    start--;
  }

  int len = cursor_pos - start;
  char prefix[256];
  if ((size_t)len >= sizeof(prefix))
    len = sizeof(prefix) - 1;
  strncpy(prefix, line + start, len);
  prefix[len] = '\0';

  if (pager->filter)
    free(pager->filter);
  pager->filter = strdup(prefix);
  pager->filter_len = len;

  bool is_first_word = (start == 0);

  if (strchr(prefix, '/')) {
    complete_paths(pager, prefix, cwd);
  } else if (is_first_word) {
    complete_commands(pager, prefix);
    complete_paths(pager, prefix, cwd);
  } else {
    /* Extract command name */
    char cmd_name[256] = {0};
    int i = 0;
    while (line[i] && !isspace(line[i]) && i < 255) {
      cmd_name[i] = line[i];
      i++;
    }
    cmd_name[i] = '\0';

    complete_flags(pager, cmd_name, prefix);
    complete_paths(pager, prefix, cwd);
  }

  if (pager->count > 0) {
    pager->active = true;
  }
}
