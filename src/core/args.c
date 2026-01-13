#include "args.h"
#include <string.h>

app_args Args_Parse(int argc, char **argv) {
  app_args args = {0};

  /* Skip argv[0] */
  for (int i = 1; i < argc && args.path_count < 2; i++) {
    strncpy(args.paths[args.path_count], argv[i], FS_MAX_PATH - 1);
    args.paths[args.path_count][FS_MAX_PATH - 1] = '\0';
    args.path_count++;
  }

  return args;
}
