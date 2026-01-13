#ifndef ARGS_H
#define ARGS_H

#include "fs.h"
#include "types.h"

typedef struct {
  char paths[2][FS_MAX_PATH];
  int path_count;
} app_args;

/* Parse command line arguments */
app_args Args_Parse(int argc, char **argv);

#endif /* ARGS_H */
