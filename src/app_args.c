#include "app_args.h"
#include "platform/platform.h"
#include <string.h>

static void ApplyPathToExplorer(explorer_state *e, const char *path) {
  char resolved[FS_MAX_PATH];
  if (FS_ResolvePath(path, resolved, sizeof(resolved))) {
    if (Platform_IsDirectory(resolved)) {
      Explorer_NavigateTo(e, resolved, false);
    } else {
      /* If it's a file, navigate to parent and select file */
      char parent[FS_MAX_PATH];
      strncpy(parent, resolved, FS_MAX_PATH);
      parent[FS_MAX_PATH - 1] = '\0';
      const char *sep = FS_FindLastSeparator(parent);
      if (sep) {
        /* Hacky way to truncate at last separator */
        ptrdiff_t offset = sep - parent;
        parent[offset] = '\0';

        Explorer_NavigateTo(e, parent, false);

        /* Find and select the file */
        const char *filename = FS_GetFilename(resolved);
        for (u32 i = 0; i < e->fs.entry_count; i++) {
          if (strcmp(e->fs.entries[i].name, filename) == 0) {
            FS_SetSelection(&e->fs, (i32)i);
            break;
          }
        }
      }
    }
    /* Sync history to just this path */
    e->history_count = 1;
    e->history_index = 0;
    strncpy(e->history[0], e->fs.current_path, FS_MAX_PATH);
  }
}

void Args_Handle(layout_state *layout, const app_args *args) {
  if (args->path_count == 0) {
    return;
  }

  printf("Args_Handle: path_count = %d\n", args->path_count);
  for (int i = 0; i < args->path_count; i++) {
    printf("  path[%d] = '%s'\n", i, args->paths[i]);
  }

  if (args->path_count >= 2) {
    /* Set dual mode first so we can navigate both panels */
    layout->mode = WB_LAYOUT_MODE_DUAL;
    layout->target_split_ratio = 0.5f;
    layout->split_ratio = 0.5f;

    ApplyPathToExplorer(&layout->panels[0].explorer, args->paths[0]);
    ApplyPathToExplorer(&layout->panels[1].explorer, args->paths[1]);

    /* Focus first panel by default */
    Layout_SetActivePanel(layout, 0);
  } else {
    /* Single path */
    ApplyPathToExplorer(&layout->panels[0].explorer, args->paths[0]);
  }
}
