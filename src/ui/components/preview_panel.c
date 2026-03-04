/*
 * preview_panel.c - Selection-driven preview pane
 *
 * Lightweight preview surface for the active explorer selection.
 */

#include "preview_panel.h"
#include "../../config/config.h"
#include "../../platform/platform.h"
#include "../../renderer/font.h"
#include "explorer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREVIEW_MIN_WIDTH 72
#define PREVIEW_MIN_LIST_WIDTH 72
#define PREVIEW_SPLITTER_WIDTH 4
#define PREVIEW_PANEL_PADDING 12
#define PREVIEW_LINE_BUFFER 1024
#define PREVIEW_MIN_RATIO 0.05f
#define PREVIEW_MAX_RATIO 0.95f

extern void *Platform_CreateThread(void *(*func)(void *), void *arg);
extern void *Platform_CreateMutex(void);
extern void Platform_LockMutex(void *mutex);
extern void Platform_UnlockMutex(void *mutex);
extern void *Platform_CreateCondVar(void);
extern void Platform_CondWait(void *cond, void *mutex);
extern void Platform_CondSignal(void *cond);

static void PreviewCopyString(char *dst, usize dst_size, const char *src) {
  usize len = 0;

  if (!dst || dst_size == 0) {
    return;
  }

  if (!src) {
    dst[0] = '\0';
    return;
  }

  len = strlen(src);
  if (len >= dst_size) {
    len = dst_size - 1;
  }

  if (len > 0) {
    memcpy(dst, src, len);
  }
  dst[len] = '\0';
}

static void PreviewContent_Clear(preview_content *content) {
  if (!content)
    return;

  if (content->text) {
    free(content->text);
  }

  if (content->img) {
    Image_Free(content->img);
  }

  memset(content, 0, sizeof(*content));
}

static void PreviewContent_Move(preview_content *dst, preview_content *src) {
  if (!dst || !src || dst == src)
    return;

  PreviewContent_Clear(dst);
  *dst = *src;
  memset(src, 0, sizeof(*src));
}

static void PreviewContent_CopyMeta(preview_content *dst,
                                    const preview_request *req) {
  PreviewCopyString(dst->path, sizeof(dst->path), req->path);
  PreviewCopyString(dst->name, sizeof(dst->name), req->name);
  dst->icon = req->icon;
  dst->is_directory = req->is_directory;
  dst->size = req->size;
  dst->modified_time = req->modified_time;
  dst->generation = req->generation;
  dst->load_kind = req->load_kind;
}

static void PreviewContent_SetSimple(preview_content *content,
                                     preview_content_type type,
                                     const fs_entry *entry, u64 generation,
                                     const char *detail) {
  PreviewContent_Clear(content);
  content->type = type;
  content->generation = generation;

  if (entry) {
    PreviewCopyString(content->path, sizeof(content->path), entry->path);
    PreviewCopyString(content->name, sizeof(content->name), entry->name);
    content->icon = entry->icon;
    content->is_directory = entry->is_directory;
    content->size = entry->size;
    content->modified_time = entry->modified_time;
  }

  if (detail) {
    PreviewCopyString(content->detail, sizeof(content->detail), detail);
  }
}

static b32 PreviewContentIsCacheable(const preview_content *content) {
  if (!content) {
    return false;
  }

  return content->type == WB_PREVIEW_CONTENT_TEXT ||
         content->type == WB_PREVIEW_CONTENT_IMAGE ||
         content->type == WB_PREVIEW_CONTENT_DIRECTORY ||
         content->type == WB_PREVIEW_CONTENT_METADATA;
}

static void PreviewPanel_PreserveCurrent(preview_state *state) {
  if (!state) {
    return;
  }

  if (PreviewContentIsCacheable(&state->current)) {
    PreviewContent_Move(&state->previous, &state->current);
  }
}

static b32 PreviewEntryEquals(preview_state *state, i32 selection_count,
                              const fs_entry *entry) {
  if (state->observed_selection_count != selection_count) {
    return false;
  }

  if (!entry) {
    return state->observed_path[0] == '\0';
  }

  return strcmp(state->observed_path, entry->path) == 0 &&
         state->observed_size == entry->size &&
         state->observed_modified_time == entry->modified_time &&
         state->observed_is_directory == entry->is_directory;
}

static void PreviewRememberSelection(preview_state *state, i32 selection_count,
                                     const fs_entry *entry) {
  state->observed_selection_count = selection_count;
  state->observed_path[0] = '\0';
  state->observed_size = 0;
  state->observed_modified_time = 0;
  state->observed_is_directory = false;

  if (entry) {
    PreviewCopyString(state->observed_path, sizeof(state->observed_path),
                      entry->path);
    state->observed_size = entry->size;
    state->observed_modified_time = entry->modified_time;
    state->observed_is_directory = entry->is_directory;
  }
}

static b32 PreviewPathLooksText(const fs_entry *entry) {
  if (!entry || entry->is_directory) {
    return false;
  }

  switch (entry->icon) {
  case WB_FILE_ICON_CODE_C:
  case WB_FILE_ICON_CODE_H:
  case WB_FILE_ICON_CODE_PY:
  case WB_FILE_ICON_CODE_JS:
  case WB_FILE_ICON_CODE_OTHER:
  case WB_FILE_ICON_DOCUMENT:
  case WB_FILE_ICON_CONFIG:
  case WB_FILE_ICON_MARKDOWN:
    return true;
  default:
    break;
  }

  {
    const char *ext = FS_GetExtension(entry->name);
    if (!ext || !ext[0]) {
      return false;
    }

    return strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".log") == 0 ||
           strcasecmp(ext, ".json") == 0 || strcasecmp(ext, ".toml") == 0 ||
           strcasecmp(ext, ".ini") == 0 || strcasecmp(ext, ".yaml") == 0 ||
           strcasecmp(ext, ".yml") == 0 || strcasecmp(ext, ".xml") == 0 ||
           strcasecmp(ext, ".csv") == 0 || strcasecmp(ext, ".md") == 0 ||
           strcasecmp(ext, ".sh") == 0 || strcasecmp(ext, ".cpp") == 0 ||
           strcasecmp(ext, ".hpp") == 0 || strcasecmp(ext, ".cc") == 0 ||
           strcasecmp(ext, ".cxx") == 0 || strcasecmp(ext, ".hxx") == 0;
  }
}

static b32 PreviewBufferLooksBinary(const u8 *data, usize size) {
  usize control_count = 0;
  usize sample = Min(size, 512);

  for (usize i = 0; i < sample; i++) {
    u8 c = data[i];
    if (c == 0) {
      return true;
    }
    if (c < 32 && c != '\n' && c != '\r' && c != '\t' && c != '\f') {
      control_count++;
    }
  }

  return sample > 0 && (control_count * 5) > sample;
}

static void PreviewLoadText(preview_state *state, const preview_request *req,
                            preview_content *result) {
  FILE *file = fopen(req->path, "rb");
  if (!file) {
    result->type = WB_PREVIEW_CONTENT_ERROR;
    PreviewCopyString(result->detail, sizeof(result->detail),
                      "Failed to read file");
    return;
  }

  usize max_bytes = (usize)Max(state->text_max_bytes, 1024);
  char *buffer = (char *)malloc(max_bytes + 1);
  if (!buffer) {
    fclose(file);
    result->type = WB_PREVIEW_CONTENT_ERROR;
    PreviewCopyString(result->detail, sizeof(result->detail), "Out of memory");
    return;
  }

  usize read_bytes = fread(buffer, 1, max_bytes, file);
  fclose(file);
  buffer[read_bytes] = '\0';

  if (PreviewBufferLooksBinary((const u8 *)buffer, read_bytes)) {
    free(buffer);
    result->type = WB_PREVIEW_CONTENT_METADATA;
    PreviewCopyString(result->detail, sizeof(result->detail), "Binary file");
    return;
  }

  result->type = WB_PREVIEW_CONTENT_TEXT;
  result->text = buffer;
  result->text_len = read_bytes;
}

static void PreviewLoadImage(preview_state *state, const preview_request *req,
                             preview_content *result) {
  if (state->image_max_decode_bytes > 0 &&
      req->size > (u64)state->image_max_decode_bytes) {
    result->type = WB_PREVIEW_CONTENT_METADATA;
    PreviewCopyString(result->detail, sizeof(result->detail),
                      "Image too large to preview");
    return;
  }

  result->img = Image_Load(req->path);
  if (!result->img) {
    result->type = WB_PREVIEW_CONTENT_ERROR;
    PreviewCopyString(result->detail, sizeof(result->detail),
                      "Failed to decode image");
    return;
  }

  if (state->image_max_dimension > 0 &&
      (result->img->width > state->image_max_dimension ||
       result->img->height > state->image_max_dimension)) {
    Image_Free(result->img);
    result->img = NULL;
    result->type = WB_PREVIEW_CONTENT_METADATA;
    PreviewCopyString(result->detail, sizeof(result->detail),
                      "Image dimensions exceed preview limit");
    return;
  }

  result->type = WB_PREVIEW_CONTENT_IMAGE;
}

static void PreviewBuildResult(preview_state *state, const preview_request *req,
                               preview_content *result) {
  memset(result, 0, sizeof(*result));
  PreviewContent_CopyMeta(result, req);

  switch (req->load_kind) {
  case WB_PREVIEW_LOAD_TEXT:
    PreviewLoadText(state, req, result);
    break;
  case WB_PREVIEW_LOAD_IMAGE:
    PreviewLoadImage(state, req, result);
    break;
  case WB_PREVIEW_LOAD_NONE:
  default:
    result->type = req->is_directory ? WB_PREVIEW_CONTENT_DIRECTORY
                                     : WB_PREVIEW_CONTENT_METADATA;
    break;
  }
}

static void *PreviewPanel_WorkerThread(void *arg) {
  preview_state *state = (preview_state *)arg;

  for (;;) {
    preview_request request = {0};

    Platform_LockMutex(state->mutex);
    while (!state->shutdown_requested && !state->worker_has_request) {
      Platform_CondWait(state->cond_var, state->mutex);
    }

    if (state->shutdown_requested) {
      Platform_UnlockMutex(state->mutex);
      break;
    }

    request = state->worker_request;
    state->worker_has_request = false;
    Platform_UnlockMutex(state->mutex);

    preview_content result;
    PreviewBuildResult(state, &request, &result);

    Platform_LockMutex(state->mutex);
    if (state->worker_has_result) {
      PreviewContent_Clear(&state->worker_result);
    }
    state->worker_result = result;
    state->worker_has_result = true;
    Platform_UnlockMutex(state->mutex);
  }

  return NULL;
}

void PreviewPanel_Init(preview_state *state) {
  memset(state, 0, sizeof(*state));
  state->splitter_id = UI_GenID("PreviewPanelSplitter");
  state->observed_selection_count = -1;
  ScrollContainer_Init(&state->scroll);
  PreviewPanel_RefreshConfig(state);

  state->mutex = Platform_CreateMutex();
  state->cond_var = Platform_CreateCondVar();
  if (state->mutex && state->cond_var) {
    state->thread = Platform_CreateThread(PreviewPanel_WorkerThread, state);
  }
}

void PreviewPanel_Shutdown(preview_state *state) {
  if (state->mutex && state->cond_var) {
    Platform_LockMutex(state->mutex);
    state->shutdown_requested = true;
    if (state->worker_has_result) {
      PreviewContent_Clear(&state->worker_result);
      state->worker_has_result = false;
    }
    Platform_CondSignal(state->cond_var);
    Platform_UnlockMutex(state->mutex);
  }
  PreviewContent_Clear(&state->current);
  PreviewContent_Clear(&state->previous);
}

void PreviewPanel_RefreshConfig(preview_state *state) {
  state->enabled = Config_GetBool("preview.enabled", false);
  state->width_ratio =
      (f32)Config_GetF64("preview.width_ratio", 0.40);
  if (state->width_ratio < PREVIEW_MIN_RATIO) {
    state->width_ratio = PREVIEW_MIN_RATIO;
  }
  if (state->width_ratio > PREVIEW_MAX_RATIO) {
    state->width_ratio = PREVIEW_MAX_RATIO;
  }

  state->text_max_bytes = Config_GetI64("preview.text.max_bytes", 262144);
  state->image_max_decode_bytes =
      Config_GetI64("preview.image.max_decode_bytes", 33554432);
  state->image_max_dimension =
      Config_GetI64("preview.image.max_dimension", 4096);
  state->selection_debounce_ms =
      Config_GetI64("preview.selection_debounce_ms", 60);
}

b32 PreviewPanel_IsVisible(preview_state *state, b32 preview_allowed) {
  return state->enabled && preview_allowed;
}

void PreviewPanel_ComputeBounds(preview_state *state, rect bounds,
                                rect *out_list_bounds,
                                rect *out_splitter_bounds,
                                rect *out_preview_bounds) {
  rect list_bounds = bounds;
  rect splitter_bounds = {bounds.x + bounds.w, bounds.y, 0, bounds.h};
  rect preview_bounds = {bounds.x + bounds.w, bounds.y, 0, bounds.h};

  i32 usable_width = bounds.w - PREVIEW_SPLITTER_WIDTH;
  if (usable_width > 0) {
    i32 min_preview = Min(PREVIEW_MIN_WIDTH, Max(usable_width - 1, 0));
    i32 min_list = Min(PREVIEW_MIN_LIST_WIDTH, Max(usable_width - min_preview, 0));
    i32 max_preview = usable_width - min_list;
    i32 preview_width = (i32)((f32)usable_width * state->width_ratio);

    if (max_preview < min_preview) {
      preview_width = Max(usable_width / 3, 0);
    } else {
      preview_width = Clamp(preview_width, min_preview, max_preview);
    }

    if (preview_width > 0 && preview_width < usable_width) {
      list_bounds.w = bounds.w - PREVIEW_SPLITTER_WIDTH - preview_width;
      splitter_bounds =
          (rect){list_bounds.x + list_bounds.w, bounds.y, PREVIEW_SPLITTER_WIDTH,
                 bounds.h};
      preview_bounds = (rect){splitter_bounds.x + splitter_bounds.w, bounds.y,
                              preview_width, bounds.h};
    }
  }

  if (out_list_bounds) {
    *out_list_bounds = list_bounds;
  }
  if (out_splitter_bounds) {
    *out_splitter_bounds = splitter_bounds;
  }
  if (out_preview_bounds) {
    *out_preview_bounds = preview_bounds;
  }
}

static void PreviewPanel_DispatchRequest(preview_state *state,
                                         const preview_request *request) {
  if (!state->mutex || !state->cond_var || !state->thread) {
    preview_content result;
    PreviewBuildResult(state, request, &result);
    if (PreviewContentIsCacheable(&state->current)) {
      PreviewContent_Move(&state->previous, &state->current);
    }
    PreviewContent_Clear(&state->current);
    PreviewContent_Move(&state->current, &result);
    return;
  }

  Platform_LockMutex(state->mutex);
  state->worker_request = *request;
  state->worker_has_request = true;
  Platform_CondSignal(state->cond_var);
  Platform_UnlockMutex(state->mutex);
}

static void PreviewPanel_ConsumeWorkerResult(preview_state *state) {
  preview_content result = {0};
  b32 has_result = false;

  if (!state->mutex) {
    return;
  }

  Platform_LockMutex(state->mutex);
  if (state->worker_has_result) {
    result = state->worker_result;
    memset(&state->worker_result, 0, sizeof(state->worker_result));
    state->worker_has_result = false;
    has_result = true;
  }
  Platform_UnlockMutex(state->mutex);

  if (!has_result) {
    return;
  }

  if (result.generation != state->current_generation) {
    PreviewContent_Clear(&result);
    return;
  }

  if (PreviewContentIsCacheable(&state->current)) {
    PreviewContent_Move(&state->previous, &state->current);
  }
  PreviewContent_Move(&state->current, &result);
  ScrollContainer_Init(&state->scroll);
}

static b32 PreviewContentMatches(const preview_content *content,
                                 const fs_entry *entry) {
  if (!content || !entry || !content->path[0]) {
    return false;
  }

  return strcmp(content->path, entry->path) == 0 && content->size == entry->size &&
         content->modified_time == entry->modified_time;
}

static preview_load_kind PreviewGetLoadKind(const fs_entry *entry) {
  if (!entry) {
    return WB_PREVIEW_LOAD_NONE;
  }
  if (entry->is_directory) {
    return WB_PREVIEW_LOAD_NONE;
  }
  if (entry->icon == WB_FILE_ICON_IMAGE) {
    return WB_PREVIEW_LOAD_IMAGE;
  }
  if (PreviewPathLooksText(entry)) {
    return WB_PREVIEW_LOAD_TEXT;
  }
  return WB_PREVIEW_LOAD_NONE;
}

static void PreviewPanel_BeginLoading(preview_state *state, const fs_entry *entry,
                                      preview_load_kind load_kind,
                                      const char *detail) {
  state->current_generation++;
  PreviewPanel_PreserveCurrent(state);
  PreviewContent_SetSimple(&state->current, WB_PREVIEW_CONTENT_LOADING, entry,
                           state->current_generation, detail);
  state->current.load_kind = load_kind;
  state->has_pending_request = true;
  memset(&state->pending_request, 0, sizeof(state->pending_request));
  state->pending_request.generation = state->current_generation;
  state->pending_request.load_kind = load_kind;
  PreviewCopyString(state->pending_request.path,
                    sizeof(state->pending_request.path), entry->path);
  PreviewCopyString(state->pending_request.name,
                    sizeof(state->pending_request.name), entry->name);
  state->pending_request.icon = entry->icon;
  state->pending_request.is_directory = entry->is_directory;
  state->pending_request.size = entry->size;
  state->pending_request.modified_time = entry->modified_time;
  state->pending_due_time_ms =
      Platform_GetTimeMs() + (u64)Max(state->selection_debounce_ms, 0);
  ScrollContainer_Init(&state->scroll);
}

static void PreviewPanel_ApplySelection(preview_state *state,
                                        struct explorer_state_s *explorer) {
  i32 selection_count = 0;
  fs_entry *entry = NULL;

  if (explorer) {
    selection_count = FS_GetSelectionCount(&explorer->fs);
    entry = FS_GetSelectedEntry(&explorer->fs);
  }

  if (PreviewEntryEquals(state, selection_count, entry)) {
    return;
  }

  PreviewRememberSelection(state, selection_count, entry);
  state->has_pending_request = false;

  if (selection_count <= 0 || !entry) {
    state->current_generation++;
    PreviewPanel_PreserveCurrent(state);
    PreviewContent_SetSimple(&state->current, WB_PREVIEW_CONTENT_EMPTY, NULL,
                             state->current_generation, "No selection");
    ScrollContainer_Init(&state->scroll);
    return;
  }

  if (selection_count > 1) {
    state->current_generation++;
    PreviewPanel_PreserveCurrent(state);
    PreviewContent_SetSimple(&state->current, WB_PREVIEW_CONTENT_MULTI_SELECTION,
                             entry, state->current_generation,
                             "Multiple items selected");
    ScrollContainer_Init(&state->scroll);
    return;
  }

  if (PreviewContentMatches(&state->current, entry) &&
      (state->current.type == WB_PREVIEW_CONTENT_TEXT ||
       state->current.type == WB_PREVIEW_CONTENT_IMAGE ||
       state->current.type == WB_PREVIEW_CONTENT_DIRECTORY ||
       state->current.type == WB_PREVIEW_CONTENT_METADATA)) {
    return;
  }

  if (PreviewContentMatches(&state->previous, entry)) {
    preview_content temp = state->current;
    state->current = state->previous;
    state->previous = temp;
    ScrollContainer_Init(&state->scroll);
    return;
  }

  if (entry->is_directory) {
    state->current_generation++;
    PreviewPanel_PreserveCurrent(state);
    PreviewContent_SetSimple(&state->current, WB_PREVIEW_CONTENT_DIRECTORY, entry,
                             state->current_generation, "Directory");
    ScrollContainer_Init(&state->scroll);
    return;
  }

  switch (PreviewGetLoadKind(entry)) {
  case WB_PREVIEW_LOAD_TEXT:
    PreviewPanel_BeginLoading(state, entry, WB_PREVIEW_LOAD_TEXT,
                              "Loading text preview...");
    break;
  case WB_PREVIEW_LOAD_IMAGE:
    PreviewPanel_BeginLoading(state, entry, WB_PREVIEW_LOAD_IMAGE,
                              "Loading image preview...");
    break;
  case WB_PREVIEW_LOAD_NONE:
  default:
    state->current_generation++;
    PreviewPanel_PreserveCurrent(state);
    PreviewContent_SetSimple(&state->current, WB_PREVIEW_CONTENT_METADATA, entry,
                             state->current_generation, "Preview not available");
    ScrollContainer_Init(&state->scroll);
    break;
  }
}

static void PreviewPanel_UpdateSplitter(preview_state *state, ui_context *ui) {
  if (state->last_splitter_bounds.w <= 0 || state->host_bounds.w <= 0) {
    return;
  }

  b32 hover = UI_PointInRect(ui->input.mouse_pos, state->last_splitter_bounds);
  if (!state->dragging_splitter && ui->active == UI_ID_NONE && hover &&
      ui->input.mouse_pressed[WB_MOUSE_LEFT]) {
    ui->active = state->splitter_id;
    state->dragging_splitter = true;
    state->drag_start_x = (f32)ui->input.mouse_pos.x;
    state->drag_start_ratio = state->width_ratio;
  }

  if (ui->active != state->splitter_id) {
    return;
  }

  if (ui->input.mouse_down[WB_MOUSE_LEFT]) {
    i32 usable_width = state->host_bounds.w - PREVIEW_SPLITTER_WIDTH;
    i32 min_preview = Min(PREVIEW_MIN_WIDTH, Max(usable_width - 1, 0));
    i32 min_list = Min(PREVIEW_MIN_LIST_WIDTH,
                       Max(usable_width - min_preview, 0));
    i32 max_preview = Max(usable_width - min_list, min_preview);
    i32 right_edge = state->host_bounds.x + state->host_bounds.w;
    i32 preview_width =
        right_edge - (ui->input.mouse_pos.x + (PREVIEW_SPLITTER_WIDTH / 2));

    if (usable_width > 0) {
      preview_width = Clamp(preview_width, min_preview, max_preview);
      state->width_ratio =
          (f32)preview_width / (f32)Max(usable_width, 1);
      state->width_ratio =
          Clamp(state->width_ratio, PREVIEW_MIN_RATIO, PREVIEW_MAX_RATIO);
    }
  } else {
    ui->active = UI_ID_NONE;
    state->dragging_splitter = false;
    Config_SetF64("preview.width_ratio", state->width_ratio);
    Config_Save();
  }
}

void PreviewPanel_Update(preview_state *state, ui_context *ui,
                         struct explorer_state_s *explorer,
                         b32 preview_allowed) {
  PreviewPanel_ConsumeWorkerResult(state);

  if (!PreviewPanel_IsVisible(state, preview_allowed)) {
    state->has_pending_request = false;
    return;
  }

  PreviewPanel_UpdateSplitter(state, ui);

  if (state->content_bounds.w > 0 && state->content_bounds.h > 0) {
    ScrollContainer_Update(&state->scroll, ui, state->content_bounds);
  }

  PreviewPanel_ApplySelection(state, explorer);

  if (state->has_pending_request &&
      Platform_GetTimeMs() >= state->pending_due_time_ms) {
    state->has_pending_request = false;
    PreviewPanel_DispatchRequest(state, &state->pending_request);
  }
}

static i32 PreviewDrawWrappedText(ui_context *ui, rect bounds, const char *text,
                                  f32 scroll_y, b32 draw_text) {
  font *font_to_use = ui->mono_font ? ui->mono_font : ui->font;
  i32 line_height = Font_GetLineHeight(font_to_use);
  i32 cell_width = Max(Font_MeasureWidth(font_to_use, "M"), 1);
  i32 columns = Max(bounds.w / cell_width, 1);
  i32 total_lines = 0;
  i32 y = bounds.y - (i32)scroll_y;
  usize at = 0;
  usize text_len = strlen(text);
  char buffer[PREVIEW_LINE_BUFFER];

  while (at < text_len) {
    i32 out_len = 0;
    while (at < text_len && text[at] != '\n' && out_len < columns &&
           out_len < (PREVIEW_LINE_BUFFER - 1)) {
      buffer[out_len++] = text[at++];
    }
    buffer[out_len] = '\0';

    if (draw_text && y + line_height >= bounds.y && y <= bounds.y + bounds.h) {
      Render_DrawText(ui->renderer, (v2i){bounds.x, y}, buffer, font_to_use,
                      ui->theme->text);
    }

    total_lines++;
    y += line_height;

    if (at < text_len && text[at] == '\n') {
      at++;
      if (out_len == 0) {
        total_lines++;
        if (draw_text && y + line_height >= bounds.y && y <= bounds.y + bounds.h) {
          Render_DrawText(ui->renderer, (v2i){bounds.x, y}, "", font_to_use,
                          ui->theme->text);
        }
        y += line_height;
      }
    }

    if (out_len == 0 && at < text_len && text[at] != '\n') {
      at++;
    }
  }

  return total_lines * line_height;
}

static void PreviewRenderMeta(ui_context *ui, rect bounds,
                              const preview_content *content) {
  const theme *th = ui->theme;
  i32 line_height = Font_GetLineHeight(ui->font);
  i32 y = bounds.y;
  char line[256];
  char size_buf[64];
  char time_buf[64];

  if (content->name[0]) {
    Render_DrawText(ui->renderer, (v2i){bounds.x, y}, content->name, ui->font,
                    th->text);
    y += line_height + 4;
  }

  snprintf(line, sizeof(line), "Type: %s",
           content->is_directory
               ? "Directory"
               : (content->type == WB_PREVIEW_CONTENT_IMAGE ? "Image"
                  : content->type == WB_PREVIEW_CONTENT_TEXT ? "Text"
                                                             : "File"));
  Render_DrawText(ui->renderer, (v2i){bounds.x, y}, line, ui->font, th->text_muted);
  y += line_height;

  if (!content->is_directory) {
    FS_FormatSize(content->size, size_buf, sizeof(size_buf));
    snprintf(line, sizeof(line), "Size: %s", size_buf);
    Render_DrawText(ui->renderer, (v2i){bounds.x, y}, line, ui->font,
                    th->text_muted);
    y += line_height;
  }

  if (content->modified_time > 0) {
    FS_FormatTime(content->modified_time, time_buf, sizeof(time_buf));
    snprintf(line, sizeof(line), "Modified: %s", time_buf);
    Render_DrawText(ui->renderer, (v2i){bounds.x, y}, line, ui->font,
                    th->text_muted);
    y += line_height;
  }

  if (content->type == WB_PREVIEW_CONTENT_IMAGE && content->img) {
    snprintf(line, sizeof(line), "Dimensions: %d x %d", content->img->width,
             content->img->height);
    Render_DrawText(ui->renderer, (v2i){bounds.x, y}, line, ui->font,
                    th->text_muted);
    y += line_height;
  }

  if (content->detail[0]) {
    Render_DrawText(ui->renderer, (v2i){bounds.x, y + 4}, content->detail,
                    ui->font, th->accent);
  }
}

void PreviewPanel_Render(preview_state *state, ui_context *ui, rect bounds) {
  render_context *ctx = ui->renderer;
  const theme *th = ui->theme;
  rect inner = {bounds.x + PREVIEW_PANEL_PADDING,
                bounds.y + PREVIEW_PANEL_PADDING,
                Max(bounds.w - (PREVIEW_PANEL_PADDING * 2), 0),
                Max(bounds.h - (PREVIEW_PANEL_PADDING * 2), 0)};
  rect meta_bounds = inner;
  i32 meta_height = Font_GetLineHeight(ui->font) * 5;

  state->content_bounds = bounds;

  Render_DrawRect(ctx, bounds, th->panel);

  if (state->last_splitter_bounds.w > 0) {
    color splitter = th->border;
    if (state->dragging_splitter) {
      splitter = th->accent;
    } else if (UI_PointInRect(ui->input.mouse_pos, state->last_splitter_bounds)) {
      splitter = th->accent_hover;
    }
    Render_DrawRect(ctx, state->last_splitter_bounds, splitter);
  }

  if (bounds.w <= 0 || bounds.h <= 0) {
    return;
  }

  PreviewRenderMeta(ui, meta_bounds, &state->current);

  if (state->current.type == WB_PREVIEW_CONTENT_TEXT && state->current.text) {
    rect text_bounds = {inner.x, inner.y + meta_height, inner.w,
                        Max(inner.h - meta_height, 0)};

    ScrollContainer_SetContentSize(
        &state->scroll,
        (f32)PreviewDrawWrappedText(ui, text_bounds, state->current.text,
                                    state->scroll.offset.y, false));

    Render_SetClipRect(ctx, text_bounds);
    PreviewDrawWrappedText(ui, text_bounds, state->current.text,
                           state->scroll.offset.y, true);
    Render_ResetClipRect(ctx);
    ScrollContainer_RenderScrollbar(&state->scroll, ui);
    return;
  }

  if (state->current.type == WB_PREVIEW_CONTENT_IMAGE && state->current.img) {
    rect image_bounds = {inner.x, inner.y + meta_height, inner.w,
                         Max(inner.h - meta_height, 0)};
    f32 scale_x = (f32)image_bounds.w / (f32)Max(state->current.img->width, 1);
    f32 scale_y = (f32)image_bounds.h / (f32)Max(state->current.img->height, 1);
    f32 scale = Min(scale_x, scale_y);
    if (scale > 1.0f) {
      scale = 1.0f;
    }
    if (scale <= 0.0f) {
      scale = 1.0f;
    }

    i32 draw_w = (i32)((f32)state->current.img->width * scale);
    i32 draw_h = (i32)((f32)state->current.img->height * scale);
    rect draw_rect = {image_bounds.x + (image_bounds.w - draw_w) / 2,
                      image_bounds.y + (image_bounds.h - draw_h) / 2, draw_w,
                      draw_h};
    Render_DrawImage(ctx, draw_rect, state->current.img,
                     COLOR_RGBA(255, 255, 255, 255));
    return;
  }
}
