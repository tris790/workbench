#ifndef PREVIEW_PANEL_H
#define PREVIEW_PANEL_H

#include "../../core/fs.h"
#include "../../core/image.h"
#include "scroll_container.h"

struct explorer_state_s;

typedef enum {
  WB_PREVIEW_CONTENT_EMPTY,
  WB_PREVIEW_CONTENT_MULTI_SELECTION,
  WB_PREVIEW_CONTENT_LOADING,
  WB_PREVIEW_CONTENT_TEXT,
  WB_PREVIEW_CONTENT_IMAGE,
  WB_PREVIEW_CONTENT_DIRECTORY,
  WB_PREVIEW_CONTENT_METADATA,
  WB_PREVIEW_CONTENT_ERROR,
} preview_content_type;

typedef enum {
  WB_PREVIEW_LOAD_NONE,
  WB_PREVIEW_LOAD_TEXT,
  WB_PREVIEW_LOAD_IMAGE,
} preview_load_kind;

typedef struct {
  preview_content_type type;
  preview_load_kind load_kind;
  u64 generation;
  char path[FS_MAX_PATH];
  char name[FS_MAX_NAME];
  file_icon_type icon;
  b32 is_directory;
  u64 size;
  u64 modified_time;
  char detail[160];
  char *text;
  usize text_len;
  image *img;
} preview_content;

typedef struct {
  u64 generation;
  preview_load_kind load_kind;
  char path[FS_MAX_PATH];
  char name[FS_MAX_NAME];
  file_icon_type icon;
  b32 is_directory;
  u64 size;
  u64 modified_time;
} preview_request;

typedef struct {
  b32 enabled;
  f32 width_ratio;
  i64 text_max_bytes;
  i64 image_max_decode_bytes;
  i64 image_max_dimension;
  i64 selection_debounce_ms;

  ui_id splitter_id;
  b32 dragging_splitter;
  f32 drag_start_x;
  f32 drag_start_ratio;
  rect host_bounds;
  rect content_bounds;
  rect last_splitter_bounds;
  scroll_container_state scroll;

  i32 observed_selection_count;
  char observed_path[FS_MAX_PATH];
  u64 observed_size;
  u64 observed_modified_time;
  b32 observed_is_directory;

  u64 current_generation;
  b32 has_pending_request;
  preview_request pending_request;
  u64 pending_due_time_ms;

  void *thread;
  void *mutex;
  void *cond_var;
  b32 shutdown_requested;
  b32 worker_has_request;
  preview_request worker_request;
  b32 worker_has_result;
  preview_content worker_result;

  preview_content current;
  preview_content previous;
} preview_state;

void PreviewPanel_Init(preview_state *state);
void PreviewPanel_Shutdown(preview_state *state);
void PreviewPanel_RefreshConfig(preview_state *state);
void PreviewPanel_Update(preview_state *state, ui_context *ui,
                         struct explorer_state_s *explorer,
                         b32 preview_allowed);
void PreviewPanel_Render(preview_state *state, ui_context *ui, rect bounds);
b32 PreviewPanel_IsVisible(preview_state *state, b32 preview_allowed);
void PreviewPanel_ComputeBounds(preview_state *state, rect bounds,
                                rect *out_list_bounds,
                                rect *out_splitter_bounds,
                                rect *out_preview_bounds);

#endif /* PREVIEW_PANEL_H */
