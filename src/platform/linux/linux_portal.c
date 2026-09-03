/*
 * linux_portal.c - Workbench FileChooser portal backend
 *
 * Workbench implements the backend interface used by xdg-desktop-portal.
 * The broker remains responsible for the public portal API and sandbox
 * integration; this module owns the actual picker UI and selected paths.
 */

#include "linux_portal.h"
#include "linux_internal.h"

#include <dbus/dbus.h>
#include <errno.h>
#include <sys/stat.h>

#define PORTAL_BUS_NAME "org.freedesktop.impl.portal.desktop.workbench"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_MAX_QUEUE 8
#define PORTAL_MAX_FILE_NAMES 64
#define PORTAL_MAX_TITLE 128
#define PORTAL_MAX_ACCEPT_LABEL 64
#define PORTAL_MAX_HANDLE 256
#define PORTAL_MAX_APP_ID 256
#define PORTAL_CONFIG_BUFFER 16384

typedef enum {
  PORTAL_REQUEST_OPEN_FILE = 0,
  PORTAL_REQUEST_OPEN_FOLDER,
  PORTAL_REQUEST_OPEN_MULTI_FILE,
  PORTAL_REQUEST_SAVE_FILE,
  PORTAL_REQUEST_SAVE_FILES,
} portal_request_mode;

typedef struct {
  b32 used;
  DBusMessage *message;
  char handle[PORTAL_MAX_HANDLE];
  char app_id[PORTAL_MAX_APP_ID];
  char title[PORTAL_MAX_TITLE];
  char accept_label[PORTAL_MAX_ACCEPT_LABEL];
  char current_folder[FS_MAX_PATH];
  char current_name[FS_MAX_NAME];
  char file_names[PORTAL_MAX_FILE_NAMES][FS_MAX_NAME];
  i32 file_name_count;
  portal_request_mode mode;
  b32 multiple;
} portal_request;

struct portal_server {
  DBusConnection *connection;
  save_dialog_state *picker;
  ui_context *ui;
  platform_window *window;
  portal_request current;
  portal_request queued[PORTAL_MAX_QUEUE];
};

static void Portal_CopyString(char *destination, usize destination_size,
                              const char *source) {
  usize length;

  if (!destination || destination_size == 0)
    return;
  if (!source) {
    destination[0] = '\0';
    return;
  }

  length = strlen(source);
  if (length >= destination_size)
    length = destination_size - 1;
  memcpy(destination, source, length);
  destination[length] = '\0';
}

static void Portal_SendMessage(portal_server *server, DBusMessage *message) {
  if (!server || !server->connection || !message)
    return;
  dbus_connection_send(server->connection, message, NULL);
  dbus_connection_flush(server->connection);
}

static void Portal_SendError(portal_server *server, DBusMessage *call,
                             const char *name, const char *text) {
  DBusMessage *reply = dbus_message_new_error(call, name, text);
  if (reply) {
    Portal_SendMessage(server, reply);
    dbus_message_unref(reply);
  }
}

static void Portal_SendEmptyReply(portal_server *server, DBusMessage *call) {
  DBusMessage *reply = dbus_message_new_method_return(call);
  if (reply) {
    Portal_SendMessage(server, reply);
    dbus_message_unref(reply);
  }
}

static b32 Portal_IsUnreserved(u8 c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
         c == '~';
}

static b32 Portal_PathToUri(const char *path, char *uri, usize uri_size) {
  static const char hex[] = "0123456789ABCDEF";
  usize out = 0;

  if (!path || !uri || uri_size < 8)
    return false;

  Portal_CopyString(uri, uri_size, "file://");
  out = strlen(uri);
  for (usize i = 0; path[i] != '\0'; i++) {
    u8 c = (u8)path[i];
    if (c == '/') {
      if (out + 1 >= uri_size)
        return false;
      uri[out++] = '/';
    } else if (Portal_IsUnreserved(c)) {
      if (out + 1 >= uri_size)
        return false;
      uri[out++] = (char)c;
    } else {
      if (out + 3 >= uri_size)
        return false;
      uri[out++] = '%';
      uri[out++] = hex[c >> 4];
      uri[out++] = hex[c & 15];
    }
  }
  uri[out] = '\0';
  return true;
}

static void Portal_AppendUriResults(DBusMessageIter *results,
                                    const portal_request *request,
                                    const save_dialog_state *picker) {
  DBusMessageIter entry;
  DBusMessageIter variant;
  DBusMessageIter array;
  const char *key = "uris";
  const char *uri_values[SAVE_DIALOG_MAX_RESULTS];
  char uri_storage[SAVE_DIALOG_MAX_RESULTS][FS_MAX_PATH * 3 + 8];
  i32 count = 0;

  if (request->mode == PORTAL_REQUEST_SAVE_FILES) {
    const char *folder = SaveDialog_GetResultPathAt(picker, 0);
    for (i32 i = 0; i < request->file_name_count &&
                    count < SAVE_DIALOG_MAX_RESULTS;
         i++) {
      char path[FS_MAX_PATH];
      FS_JoinPath(path, sizeof(path), folder, request->file_names[i]);
      if (Portal_PathToUri(path, uri_storage[count], sizeof(uri_storage[count]))) {
        uri_values[count] = uri_storage[count];
        count++;
      }
    }
  } else {
    i32 result_count = SaveDialog_GetResultCount(picker);
    for (i32 i = 0; i < result_count && count < SAVE_DIALOG_MAX_RESULTS; i++) {
      const char *path = SaveDialog_GetResultPathAt(picker, i);
      if (Portal_PathToUri(path, uri_storage[count], sizeof(uri_storage[count]))) {
        uri_values[count] = uri_storage[count];
        count++;
      }
    }
  }

  dbus_message_iter_open_container(results, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as", &variant);
  dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &array);
  for (i32 i = 0; i < count; i++) {
    const char *uri = uri_values[i];
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &uri);
  }
  dbus_message_iter_close_container(&variant, &array);
  dbus_message_iter_close_container(&entry, &variant);
  dbus_message_iter_close_container(results, &entry);
}

static void Portal_SendPickerReply(portal_server *server,
                                   portal_request *request, b32 accepted) {
  DBusMessage *reply;
  DBusMessageIter args;
  DBusMessageIter results;
  u32 response = accepted ? 0 : 1;

  reply = dbus_message_new_method_return(request->message);
  if (!reply)
    return;

  dbus_message_iter_init_append(reply, &args);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &response);
  dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &results);
  if (accepted)
    Portal_AppendUriResults(&results, request, server->picker);
  dbus_message_iter_close_container(&args, &results);

  Portal_SendMessage(server, reply);
  dbus_message_unref(reply);
  dbus_message_unref(request->message);
  memset(request, 0, sizeof(*request));
}

static void Portal_ReadByteArray(DBusMessageIter *value, char *destination,
                                 usize destination_size) {
  DBusMessageIter bytes;
  usize out = 0;

  if (!destination || destination_size == 0)
    return;
  destination[0] = '\0';
  if (dbus_message_iter_get_arg_type(value) != DBUS_TYPE_ARRAY)
    return;

  dbus_message_iter_recurse(value, &bytes);
  while (dbus_message_iter_get_arg_type(&bytes) != DBUS_TYPE_INVALID) {
    unsigned char byte = 0;
    if (dbus_message_iter_get_arg_type(&bytes) != DBUS_TYPE_BYTE)
      break;
    dbus_message_iter_get_basic(&bytes, &byte);
    if (byte == 0)
      break;
    if (out + 1 < destination_size)
      destination[out++] = (char)byte;
    dbus_message_iter_next(&bytes);
  }
  destination[out] = '\0';
}

static void Portal_ReadFileNames(DBusMessageIter *value,
                                 portal_request *request) {
  DBusMessageIter files;

  if (dbus_message_iter_get_arg_type(value) != DBUS_TYPE_ARRAY)
    return;
  dbus_message_iter_recurse(value, &files);
  while (dbus_message_iter_get_arg_type(&files) != DBUS_TYPE_INVALID &&
         request->file_name_count < PORTAL_MAX_FILE_NAMES) {
    char name[FS_MAX_NAME];
    Portal_ReadByteArray(&files, name, sizeof(name));
    if (name[0]) {
      Portal_CopyString(request->file_names[request->file_name_count],
                        FS_MAX_NAME, FS_GetFilename(name));
      request->file_name_count++;
    }
    dbus_message_iter_next(&files);
  }
}

static void Portal_ParseOptions(DBusMessageIter *options,
                                portal_request *request) {
  DBusMessageIter entries;

  if (dbus_message_iter_get_arg_type(options) != DBUS_TYPE_ARRAY)
    return;
  dbus_message_iter_recurse(options, &entries);
  while (dbus_message_iter_get_arg_type(&entries) != DBUS_TYPE_INVALID) {
    DBusMessageIter entry;
    DBusMessageIter variant;
    const char *key = NULL;

    if (dbus_message_iter_get_arg_type(&entries) != DBUS_TYPE_DICT_ENTRY)
      break;
    dbus_message_iter_recurse(&entries, &entry);
    if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
      break;
    dbus_message_iter_get_basic(&entry, &key);
    if (!dbus_message_iter_next(&entry) ||
        dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
      break;
    dbus_message_iter_recurse(&entry, &variant);

    if (strcmp(key, "multiple") == 0 &&
        dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_BOOLEAN) {
      dbus_bool_t value = FALSE;
      dbus_message_iter_get_basic(&variant, &value);
      request->multiple = value != FALSE;
    } else if (strcmp(key, "directory") == 0 &&
               dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_BOOLEAN) {
      dbus_bool_t value = FALSE;
      dbus_message_iter_get_basic(&variant, &value);
      if (value != FALSE)
        request->mode = PORTAL_REQUEST_OPEN_FOLDER;
    } else if (strcmp(key, "current_folder") == 0) {
      Portal_ReadByteArray(&variant, request->current_folder,
                           sizeof(request->current_folder));
    } else if (strcmp(key, "current_name") == 0 &&
               dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING) {
      const char *value = NULL;
      dbus_message_iter_get_basic(&variant, &value);
      Portal_CopyString(request->current_name, sizeof(request->current_name),
                        value);
    } else if (strcmp(key, "accept_label") == 0 &&
               dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING) {
      const char *value = NULL;
      dbus_message_iter_get_basic(&variant, &value);
      Portal_CopyString(request->accept_label, sizeof(request->accept_label),
                        value);
    } else if (strcmp(key, "current_file") == 0) {
      char current_file[FS_MAX_PATH];
      Portal_ReadByteArray(&variant, current_file, sizeof(current_file));
      if (current_file[0]) {
        const char *filename = FS_GetFilename(current_file);
        Portal_CopyString(request->current_name, sizeof(request->current_name),
                          filename);
        if (filename > current_file) {
          usize length = (usize)(filename - current_file - 1);
          if (length >= sizeof(request->current_folder))
            length = sizeof(request->current_folder) - 1;
          memcpy(request->current_folder, current_file, length);
          request->current_folder[length] = '\0';
        }
      }
    } else if (strcmp(key, "files") == 0) {
      Portal_ReadFileNames(&variant, request);
    }

    dbus_message_iter_next(&entries);
  }
}

static b32 Portal_PrepareRequest(portal_request *request, const char *member,
                                 DBusMessage *message) {
  DBusMessageIter args;
  const char *handle = NULL;
  const char *app_id = NULL;
  const char *parent_window = NULL;
  const char *title = NULL;

  (void)parent_window;
  request->message = dbus_message_ref(message);
  if (!dbus_message_iter_init(message, &args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_OBJECT_PATH)
    return false;

  dbus_message_iter_get_basic(&args, &handle);
  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
    return false;
  dbus_message_iter_get_basic(&args, &app_id);
  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
    return false;
  dbus_message_iter_get_basic(&args, &parent_window);
  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
    return false;
  dbus_message_iter_get_basic(&args, &title);
  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY)
    return false;

  Portal_CopyString(request->handle, sizeof(request->handle), handle);
  Portal_CopyString(request->app_id, sizeof(request->app_id), app_id);
  Portal_CopyString(request->title, sizeof(request->title), title);
  request->accept_label[0] = '\0';
  if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY)
    Portal_ParseOptions(&args, request);

  if (strcmp(member, "OpenFile") == 0) {
    if (request->mode == PORTAL_REQUEST_OPEN_FOLDER)
      request->mode = PORTAL_REQUEST_OPEN_FOLDER;
    else if (request->multiple)
      request->mode = PORTAL_REQUEST_OPEN_MULTI_FILE;
    else
      request->mode = PORTAL_REQUEST_OPEN_FILE;
  } else if (strcmp(member, "SaveFile") == 0) {
    request->mode = PORTAL_REQUEST_SAVE_FILE;
  } else {
    request->mode = PORTAL_REQUEST_SAVE_FILES;
  }
  return true;
}

static void Portal_StartRequest(portal_server *server, portal_request *request) {
  save_dialog_mode mode;

  server->current = *request;
  memset(request, 0, sizeof(*request));

  switch (server->current.mode) {
  case PORTAL_REQUEST_OPEN_FILE:
    mode = WB_PICKER_OPEN_FILE;
    break;
  case PORTAL_REQUEST_OPEN_FOLDER:
    mode = WB_PICKER_OPEN_FOLDER;
    break;
  case PORTAL_REQUEST_OPEN_MULTI_FILE:
    mode = WB_PICKER_OPEN_MULTI_FILE;
    break;
  case PORTAL_REQUEST_SAVE_FILES:
    mode = WB_PICKER_SAVE_FILES;
    break;
  case PORTAL_REQUEST_SAVE_FILE:
  default:
    mode = WB_PICKER_SAVE_FILE;
    break;
  }

  SaveDialog_OpenPicker(server->picker, mode, server->current.title,
                        server->current.current_folder,
                        server->current.current_name,
                        server->current.accept_label, server->current.multiple);

  /* Portal requests can arrive while Workbench is behind the requesting app.
   * Ask the Wayland compositor to activate our existing surface before the
   * picker starts receiving input. */
  Platform_ActivateWindow(server->window);
}

static b32 Portal_QueueRequest(portal_server *server, portal_request *request) {
  if (!server->current.used) {
    request->used = true;
    Portal_StartRequest(server, request);
    return true;
  }

  for (i32 i = 0; i < PORTAL_MAX_QUEUE; i++) {
    if (!server->queued[i].used) {
      request->used = true;
      server->queued[i] = *request;
      memset(request, 0, sizeof(*request));
      return true;
    }
  }
  return false;
}

static portal_request *Portal_FindRequest(portal_server *server,
                                          const char *path) {
  if (server->current.used && strcmp(server->current.handle, path) == 0)
    return &server->current;
  for (i32 i = 0; i < PORTAL_MAX_QUEUE; i++) {
    if (server->queued[i].used && strcmp(server->queued[i].handle, path) == 0)
      return &server->queued[i];
  }
  return NULL;
}

static DBusHandlerResult Portal_Filter(DBusConnection *connection,
                                       DBusMessage *message, void *user_data) {
  portal_server *server = (portal_server *)user_data;
  const char *interface = dbus_message_get_interface(message);
  const char *member = dbus_message_get_member(message);
  const char *path = dbus_message_get_path(message);

  (void)connection;
  if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (interface && strcmp(interface, "org.freedesktop.impl.portal.FileChooser") == 0 &&
      path && strcmp(path, PORTAL_OBJECT_PATH) == 0 && member &&
      (strcmp(member, "OpenFile") == 0 || strcmp(member, "SaveFile") == 0 ||
       strcmp(member, "SaveFiles") == 0)) {
    portal_request request = {0};
    if (!Portal_PrepareRequest(&request, member, message) ||
        !request.handle[0]) {
      Portal_SendError(server, message, "org.freedesktop.portal.Error.Failed",
                       "Invalid FileChooser request");
      if (request.message)
        dbus_message_unref(request.message);
    } else if (!Portal_QueueRequest(server, &request)) {
      Portal_SendError(server, message, "org.freedesktop.portal.Error.Busy",
                       "Workbench already has too many file picker requests");
      dbus_message_unref(request.message);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (interface && strcmp(interface, "org.freedesktop.impl.portal.Request") == 0 &&
      member && strcmp(member, "Close") == 0 && path) {
    portal_request *request = Portal_FindRequest(server, path);
    if (request == &server->current) {
      SaveDialog_Cancel(server->picker, server->ui);
    } else if (request) {
      dbus_message_unref(request->message);
      memset(request, 0, sizeof(*request));
    }
    Portal_SendEmptyReply(server, message);
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (interface && strcmp(interface, "org.freedesktop.DBus.Introspectable") == 0 &&
      member && strcmp(member, "Introspect") == 0) {
    static const char introspection[] =
        "<node>"
        "<interface name='org.freedesktop.impl.portal.FileChooser'>"
        "<method name='OpenFile'><arg direction='in' type='o'/>"
        "<arg direction='in' type='s'/><arg direction='in' type='s'/>"
        "<arg direction='in' type='s'/><arg direction='in' type='a{sv}'/>"
        "<arg direction='out' type='u'/><arg direction='out' type='a{sv}'/>"
        "</method>"
        "<method name='SaveFile'><arg direction='in' type='o'/>"
        "<arg direction='in' type='s'/><arg direction='in' type='s'/>"
        "<arg direction='in' type='s'/><arg direction='in' type='a{sv}'/>"
        "<arg direction='out' type='u'/><arg direction='out' type='a{sv}'/>"
        "</method>"
        "<method name='SaveFiles'><arg direction='in' type='o'/>"
        "<arg direction='in' type='s'/><arg direction='in' type='s'/>"
        "<arg direction='in' type='s'/><arg direction='in' type='a{sv}'/>"
        "<arg direction='out' type='u'/><arg direction='out' type='a{sv}'/>"
        "</method></interface>"
        "<interface name='org.freedesktop.impl.portal.Request'>"
        "<method name='Close'/></interface></node>";
    DBusMessage *reply = dbus_message_new_method_return(message);
    DBusMessageIter args;
    const char *xml = introspection;
    if (reply) {
      dbus_message_iter_init_append(reply, &args);
      dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &xml);
      Portal_SendMessage(server, reply);
      dbus_message_unref(reply);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

b32 Portal_Init(portal_server *server, save_dialog_state *picker,
                ui_context *ui, platform_window *window) {
  DBusError error;
  int request_result;

  if (!server || !picker || !ui || !window)
    return false;
  memset(server, 0, sizeof(*server));
  server->picker = picker;
  server->ui = ui;
  server->window = window;

  dbus_error_init(&error);
  server->connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
  if (!server->connection) {
    fprintf(stderr, "Workbench portal: unable to connect to session bus: %s\n",
            error.message ? error.message : "unknown error");
    dbus_error_free(&error);
    return false;
  }
  dbus_connection_set_exit_on_disconnect(server->connection, FALSE);
  request_result = dbus_bus_request_name(server->connection, PORTAL_BUS_NAME,
                                         DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
  if (request_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    fprintf(stderr, "Workbench portal: unable to own D-Bus name: %s\n",
            error.message ? error.message : "name is already owned");
    dbus_error_free(&error);
    dbus_connection_unref(server->connection);
    server->connection = NULL;
    return false;
  }
  dbus_error_free(&error);

  if (!dbus_connection_add_filter(server->connection, Portal_Filter, server,
                                  NULL)) {
    dbus_bus_release_name(server->connection, PORTAL_BUS_NAME, NULL);
    dbus_connection_unref(server->connection);
    server->connection = NULL;
    return false;
  }
  return true;
}

b32 Portal_Create(portal_server **out_server, memory_arena *arena) {
  portal_server *server;

  if (!out_server || !arena)
    return false;
  server = ArenaPushStruct(arena, portal_server);
  if (!server)
    return false;
  memset(server, 0, sizeof(*server));
  *out_server = server;
  return true;
}

void Portal_Poll(portal_server *server) {
  if (!server || !server->connection)
    return;

  dbus_connection_read_write(server->connection, 0);
  while (dbus_connection_get_dispatch_status(server->connection) ==
         DBUS_DISPATCH_DATA_REMAINS) {
    dbus_connection_dispatch(server->connection);
  }
}

void Portal_Complete(portal_server *server) {
  if (!server || !server->current.used ||
      SaveDialog_IsOpen(server->picker) ||
      !SaveDialog_IsFinished(server->picker))
    return;

  Portal_SendPickerReply(server, &server->current,
                         SaveDialog_WasAccepted(server->picker));

  for (i32 i = 0; i < PORTAL_MAX_QUEUE; i++) {
    if (server->queued[i].used) {
      portal_request next = server->queued[i];
      memset(&server->queued[i], 0, sizeof(server->queued[i]));
      Portal_StartRequest(server, &next);
      break;
    }
  }
}

void Portal_CancelAll(portal_server *server) {
  if (!server)
    return;
  if (server->current.used) {
    SaveDialog_Cancel(server->picker, server->ui);
    if (server->current.used)
      Portal_SendPickerReply(server, &server->current, false);
  }
  for (i32 i = 0; i < PORTAL_MAX_QUEUE; i++) {
    if (server->queued[i].used) {
      Portal_SendPickerReply(server, &server->queued[i], false);
    }
  }
}

void Portal_Shutdown(portal_server *server) {
  if (!server)
    return;
  Portal_CancelAll(server);
  if (server->connection) {
    dbus_bus_release_name(server->connection, PORTAL_BUS_NAME, NULL);
    dbus_connection_remove_filter(server->connection, Portal_Filter, server);
    dbus_connection_unref(server->connection);
  }
  memset(server, 0, sizeof(*server));
}

void Portal_Destroy(portal_server *server) {
  if (!server)
    return;
  Portal_Shutdown(server);
}

static b32 Portal_EnsureDirectory(const char *path) {
  char buffer[FS_MAX_PATH];
  usize length;

  if (!path || !path[0])
    return false;
  Portal_CopyString(buffer, sizeof(buffer), path);
  length = strlen(buffer);
  if (length == 0)
    return false;
  if (buffer[length - 1] == '/')
    buffer[length - 1] = '\0';

  for (char *p = buffer + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(buffer, 0700) != 0 && errno != EEXIST)
        return false;
      *p = '/';
    }
  }
  if (mkdir(buffer, 0700) != 0 && errno != EEXIST)
    return false;
  return true;
}

static b32 Portal_WriteFile(const char *path, const char *contents) {
  FILE *file = fopen(path, "w");
  if (!file)
    return false;
  fputs(contents, file);
  if (fclose(file) != 0)
    return false;
  return true;
}

static b32 Portal_UpdateConfig(const char *path) {
  FILE *file;
  char original[PORTAL_CONFIG_BUFFER];
  char updated[PORTAL_CONFIG_BUFFER];
  usize original_length = 0;
  usize updated_length = 0;
  b32 replaced = false;
  b32 has_preferred_section = false;
  const char *key = "org.freedesktop.impl.portal.FileChooser=";

  file = fopen(path, "r");
  if (file) {
    original_length = fread(original, 1, sizeof(original) - 1, file);
    fclose(file);
    original[original_length] = '\0';
  } else if (errno != ENOENT) {
    return false;
  } else {
    original[0] = '\0';
  }

  const char *cursor = original;
  while (*cursor && updated_length + 1 < sizeof(updated)) {
    const char *line_end = strchr(cursor, '\n');
    usize line_length = line_end ? (usize)(line_end - cursor) : strlen(cursor);
    if (line_length == strlen("[preferred]") &&
        strncmp(cursor, "[preferred]", line_length) == 0)
      has_preferred_section = true;
    if (strncmp(cursor, key, strlen(key)) == 0) {
      const char replacement[] =
          "org.freedesktop.impl.portal.FileChooser=workbench\n";
      usize replacement_length = sizeof(replacement) - 1;
      if (updated_length + replacement_length >= sizeof(updated))
        return false;
      memcpy(updated + updated_length, replacement, replacement_length);
      updated_length += replacement_length;
      replaced = true;
    } else {
      if (updated_length + line_length + (line_end ? 1 : 0) >=
          sizeof(updated))
        return false;
      memcpy(updated + updated_length, cursor, line_length);
      updated_length += line_length;
      if (line_end)
        updated[updated_length++] = '\n';
    }
    cursor = line_end ? line_end + 1 : cursor + line_length;
  }

  if (!replaced) {
    const char *addition = has_preferred_section
                               ? "org.freedesktop.impl.portal.FileChooser=workbench\n"
                               : "\n[preferred]\n"
                                 "org.freedesktop.impl.portal.FileChooser=workbench\n";
    usize addition_length = strlen(addition);
    if (updated_length + addition_length >= sizeof(updated))
      return false;
    memcpy(updated + updated_length, addition, addition_length);
    updated_length += addition_length;
  }
  updated[updated_length] = '\0';
  return Portal_WriteFile(path, updated);
}

static b32 Portal_JoinPath(char *destination, usize destination_size,
                           const char *base, const char *suffix) {
  usize base_length;
  usize suffix_length;

  if (!destination || !base || !suffix || destination_size == 0)
    return false;
  base_length = strlen(base);
  suffix_length = strlen(suffix);
  if (base_length + suffix_length + 1 > destination_size)
    return false;
  memcpy(destination, base, base_length);
  memcpy(destination + base_length, suffix, suffix_length + 1);
  return true;
}

static b32 Portal_ReloadBusConfiguration(void) {
  DBusError error;
  DBusConnection *connection;
  DBusMessage *call;
  DBusMessage *reply;

  dbus_error_init(&error);
  connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
  if (!connection) {
    if (dbus_error_is_set(&error)) {
      fprintf(stderr, "Workbench portal: unable to connect to user D-Bus: %s\n",
              error.message);
      dbus_error_free(&error);
    }
    return false;
  }

  call = dbus_message_new_method_call(
      "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "ReloadConfig");
  if (!call) {
    dbus_connection_unref(connection);
    return false;
  }

  reply = dbus_connection_send_with_reply_and_block(connection, call, 2000,
                                                     &error);
  dbus_message_unref(call);
  dbus_connection_unref(connection);
  if (!reply) {
    if (dbus_error_is_set(&error)) {
      fprintf(stderr, "Workbench portal: unable to reload user D-Bus: %s\n",
              error.message);
      dbus_error_free(&error);
    }
    return false;
  }

  dbus_message_unref(reply);
  return true;
}

b32 Portal_Install(void) {
  const char *home = getenv("HOME");
  const char *data_home = getenv("XDG_DATA_HOME");
  const char *config_home = getenv("XDG_CONFIG_HOME");
  char data_root[FS_MAX_PATH];
  char config_root[FS_MAX_PATH];
  char portal_dir[FS_MAX_PATH];
  char service_dir[FS_MAX_PATH];
  char config_dir[FS_MAX_PATH];
  char portal_path[FS_MAX_PATH];
  char service_path[FS_MAX_PATH];
  char config_path[FS_MAX_PATH];
  char executable[FS_MAX_PATH];
  ssize_t executable_length;
  char service_contents[FS_MAX_PATH + 128];

  if (!home || !home[0]) {
    fprintf(stderr, "Workbench portal: HOME is not set\n");
    return false;
  }
  if (!data_home || !data_home[0]) {
    snprintf(data_root, sizeof(data_root), "%s/.local/share", home);
    data_home = data_root;
  }
  if (!config_home || !config_home[0]) {
    snprintf(config_root, sizeof(config_root), "%s/.config", home);
    config_home = config_root;
  }

  executable_length = readlink("/proc/self/exe", executable,
                              sizeof(executable) - 1);
  if (executable_length < 0) {
    fprintf(stderr, "Workbench portal: unable to locate executable\n");
    return false;
  }
  executable[executable_length] = '\0';

  if (!Portal_JoinPath(portal_dir, sizeof(portal_dir), data_home,
                       "/xdg-desktop-portal/portals") ||
      !Portal_JoinPath(service_dir, sizeof(service_dir), data_home,
                       "/dbus-1/services") ||
      !Portal_JoinPath(config_dir, sizeof(config_dir), config_home,
                       "/xdg-desktop-portal") ||
      !Portal_EnsureDirectory(portal_dir) ||
      !Portal_EnsureDirectory(service_dir) ||
      !Portal_EnsureDirectory(config_dir)) {
    fprintf(stderr, "Workbench portal: unable to create user directories\n");
    return false;
  }

  if (!Portal_JoinPath(portal_path, sizeof(portal_path), portal_dir,
                       "/workbench.portal") ||
      !Portal_JoinPath(service_path, sizeof(service_path), service_dir,
                       "/org.freedesktop.impl.portal.desktop.workbench.service") ||
      !Portal_JoinPath(config_path, sizeof(config_path), config_dir,
                       "/portals.conf") ||
      !Portal_WriteFile(
          portal_path,
          "[portal]\n"
          "DBusName=org.freedesktop.impl.portal.desktop.workbench\n"
          "Interfaces=org.freedesktop.impl.portal.FileChooser\n") ||
      (snprintf(service_contents, sizeof(service_contents),
                "[D-BUS Service]\n"
                "Name=org.freedesktop.impl.portal.desktop.workbench\n"
                "Exec=%s --portal\n",
                executable),
       !Portal_WriteFile(service_path, service_contents)) ||
      !Portal_UpdateConfig(config_path)) {
    fprintf(stderr, "Workbench portal: unable to install registration files\n");
    return false;
  }

  if (Portal_ReloadBusConfiguration()) {
    printf("Workbench FileChooser portal installed for this user.\n");
    printf("D-Bus activation registration reloaded.\n");
  } else {
    printf("Workbench FileChooser portal installed for this user.\n");
    printf("Run 'busctl --user call org.freedesktop.DBus /org/freedesktop/DBus "
           "org.freedesktop.DBus ReloadConfig' or log in again to apply it.\n");
  }
  return true;
}
