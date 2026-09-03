#ifndef ARGS_H
#define ARGS_H

#include "fs.h"
#include "types.h"

typedef struct {
  char paths[2][FS_MAX_PATH];
  int path_count;

  /* External picker and portal service modes. */
  b32 save_mode;
  b32 save_pick_only;
  i32 picker_mode;
  b32 portal_mode;
  b32 install_portal;
  char save_source[FS_MAX_PATH];
  char save_name[FS_MAX_NAME];
  char save_folder[FS_MAX_PATH];
  char save_mime[128];
  char picker_title[128];
  char picker_accept[64];
} app_args;

/* Values intentionally mirror save_dialog_mode without making core depend on
 * the UI layer. */
#define WB_APP_PICKER_NONE 0
#define WB_APP_PICKER_SAVE_FILE 1
#define WB_APP_PICKER_OPEN_FILE 2
#define WB_APP_PICKER_OPEN_FOLDER 3
#define WB_APP_PICKER_OPEN_MULTI_FILE 4
#define WB_APP_PICKER_SAVE_FILES 5

/* Parse command line arguments */
app_args Args_Parse(int argc, char **argv);

#endif /* ARGS_H */
