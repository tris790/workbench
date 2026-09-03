#include "args.h"
#include <string.h>

app_args Args_Parse(int argc, char **argv) {
  app_args args = {0};

  /* Skip argv[0].  Unknown options remain positional paths for backwards
   * compatibility with the original explorer launcher. */
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    const char *value = NULL;

    if (strcmp(arg, "--save") == 0) {
      args.save_mode = true;
      args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      continue;
    }

    if (strcmp(arg, "--open") == 0) {
      args.picker_mode = WB_APP_PICKER_OPEN_FILE;
      continue;
    }

    if (strcmp(arg, "--open-folder") == 0 ||
        strcmp(arg, "--pick-folder") == 0) {
      args.picker_mode = WB_APP_PICKER_OPEN_FOLDER;
      continue;
    }

    if (strcmp(arg, "--open-multiple") == 0 ||
        strcmp(arg, "--open-multi") == 0) {
      args.picker_mode = WB_APP_PICKER_OPEN_MULTI_FILE;
      continue;
    }

    if (strcmp(arg, "--save-files") == 0) {
      args.save_mode = true;
      args.picker_mode = WB_APP_PICKER_SAVE_FILES;
      continue;
    }

    if (strcmp(arg, "--portal") == 0) {
      args.portal_mode = true;
      continue;
    }

    if (strcmp(arg, "--install-portal") == 0) {
      args.install_portal = true;
      continue;
    }

    if (strcmp(arg, "--pick") == 0 || strcmp(arg, "--pick-save") == 0) {
      args.save_mode = true;
      args.save_pick_only = true;
      args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      continue;
    }

    if ((strcmp(arg, "--source") == 0 ||
         strcmp(arg, "--save-source") == 0) && i + 1 < argc) {
      args.save_mode = true;
      args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      value = argv[++i];
      strncpy(args.save_source, value, FS_MAX_PATH - 1);
      args.save_source[FS_MAX_PATH - 1] = '\0';
      continue;
    }

    if ((strcmp(arg, "--name") == 0 || strcmp(arg, "--save-name") == 0) &&
        i + 1 < argc) {
      if (strcmp(arg, "--save-name") == 0 || args.picker_mode == WB_APP_PICKER_NONE) {
        args.save_mode = true;
        args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      }
      value = argv[++i];
      strncpy(args.save_name, value, FS_MAX_NAME - 1);
      args.save_name[FS_MAX_NAME - 1] = '\0';
      continue;
    }

    if ((strcmp(arg, "--folder") == 0 || strcmp(arg, "--save-folder") == 0) &&
        i + 1 < argc) {
      if (strcmp(arg, "--save-folder") == 0 || args.picker_mode == WB_APP_PICKER_NONE) {
        args.save_mode = true;
        args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      }
      value = argv[++i];
      strncpy(args.save_folder, value, FS_MAX_PATH - 1);
      args.save_folder[FS_MAX_PATH - 1] = '\0';
      continue;
    }

    if (strcmp(arg, "--mime") == 0 && i + 1 < argc) {
      args.save_mode = true;
      args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      value = argv[++i];
      strncpy(args.save_mime, value, sizeof(args.save_mime) - 1);
      args.save_mime[sizeof(args.save_mime) - 1] = '\0';
      continue;
    }

    if (strcmp(arg, "--title") == 0 && i + 1 < argc) {
      value = argv[++i];
      strncpy(args.picker_title, value, sizeof(args.picker_title) - 1);
      args.picker_title[sizeof(args.picker_title) - 1] = '\0';
      continue;
    }

    if (strcmp(arg, "--accept") == 0 && i + 1 < argc) {
      value = argv[++i];
      strncpy(args.picker_accept, value, sizeof(args.picker_accept) - 1);
      args.picker_accept[sizeof(args.picker_accept) - 1] = '\0';
      continue;
    }

    /* Also accept --name=value and --source=value for URI/helper wrappers. */
    if (strncmp(arg, "--name=", 7) == 0 ||
        strncmp(arg, "--save-name=", 12) == 0) {
      const char *start = strchr(arg, '=') + 1;
      if (strncmp(arg, "--save-name=", 12) == 0 ||
          args.picker_mode == WB_APP_PICKER_NONE) {
        args.save_mode = true;
        args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      }
      strncpy(args.save_name, start, FS_MAX_NAME - 1);
      args.save_name[FS_MAX_NAME - 1] = '\0';
      continue;
    }

    if (strncmp(arg, "--folder=", 9) == 0 ||
        strncmp(arg, "--save-folder=", 14) == 0) {
      const char *start = strchr(arg, '=') + 1;
      if (strncmp(arg, "--save-folder=", 14) == 0 ||
          args.picker_mode == WB_APP_PICKER_NONE) {
        args.save_mode = true;
        args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      }
      strncpy(args.save_folder, start, FS_MAX_PATH - 1);
      args.save_folder[FS_MAX_PATH - 1] = '\0';
      continue;
    }

    if (strncmp(arg, "--source=", 9) == 0 ||
        strncmp(arg, "--save-source=", 14) == 0) {
      const char *start = strchr(arg, '=') + 1;
      args.save_mode = true;
      args.picker_mode = WB_APP_PICKER_SAVE_FILE;
      strncpy(args.save_source, start, FS_MAX_PATH - 1);
      args.save_source[FS_MAX_PATH - 1] = '\0';
      continue;
    }

    if (strncmp(arg, "--title=", 8) == 0) {
      strncpy(args.picker_title, arg + 8, sizeof(args.picker_title) - 1);
      args.picker_title[sizeof(args.picker_title) - 1] = '\0';
      continue;
    }

    if (strncmp(arg, "--accept=", 9) == 0) {
      strncpy(args.picker_accept, arg + 9, sizeof(args.picker_accept) - 1);
      args.picker_accept[sizeof(args.picker_accept) - 1] = '\0';
      continue;
    }

    if (args.path_count < 2) {
      strncpy(args.paths[args.path_count], arg, FS_MAX_PATH - 1);
      args.paths[args.path_count][FS_MAX_PATH - 1] = '\0';
      args.path_count++;
    }
  }

  return args;
}
