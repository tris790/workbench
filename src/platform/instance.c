/*
 * instance.c - Cross-platform single-instance command-line handoff.
 *
 * The wire format is deliberately tiny and private: a u32 argument count,
 * followed by u32 byte lengths and UTF-8 argument bytes.  This keeps paths
 * with spaces intact on both platforms and avoids shell quoting differences.
 */

#include "platform.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INSTANCE_WIRE_SIZE 8192
#define INSTANCE_MAX_ARGS 64

static usize Instance_EncodeArguments(int argc, char **argv, u8 *buffer,
                                      usize buffer_size) {
  u32 count = (argc > 1) ? (u32)(argc - 1) : 0;
  usize offset = sizeof(count);

  if (!buffer || buffer_size < sizeof(count) || count > INSTANCE_MAX_ARGS)
    return 0;
  memcpy(buffer, &count, sizeof(count));
  for (u32 i = 0; i < count; i++) {
    u32 length = (u32)strlen(argv[i + 1]);
    if (offset + sizeof(length) + length > buffer_size)
      return 0;
    memcpy(buffer + offset, &length, sizeof(length));
    offset += sizeof(length);
    memcpy(buffer + offset, argv[i + 1], length);
    offset += length;
  }
  return offset;
}

static b32 Instance_DecodeArguments(const u8 *wire, usize wire_size, i32 *argc,
                                    char **argv, i32 max_args, char *storage,
                                    usize storage_size) {
  u32 count;
  usize wire_offset = 0;
  usize storage_offset = 0;

  if (!wire || wire_size < sizeof(count) || !argc || !argv || !storage ||
      max_args < 1)
    return false;
  memcpy(&count, wire, sizeof(count));
  wire_offset += sizeof(count);
  if (count > (u32)(max_args - 1))
    return false;

  argv[0] = (char *)"workbench";
  for (u32 i = 0; i < count; i++) {
    u32 length;
    if (wire_offset + sizeof(length) > wire_size)
      return false;
    memcpy(&length, wire + wire_offset, sizeof(length));
    wire_offset += sizeof(length);
    if (wire_offset + length > wire_size ||
        storage_offset + length + 1 > storage_size)
      return false;
    memcpy(storage + storage_offset, wire + wire_offset, length);
    storage[storage_offset + length] = '\0';
    argv[i + 1] = storage + storage_offset;
    storage_offset += length + 1;
    wire_offset += length;
  }
  *argc = (i32)count + 1;
  return true;
}

#ifdef _WIN32

#include "windows/windows_internal.h"

struct platform_instance {
  HANDLE mutex;
  HWND window;
  u8 pending[INSTANCE_WIRE_SIZE];
  usize pending_size;
};

void Platform_InstanceReceive(const void *data, usize size) {
  platform_instance *instance = g_platform.single_instance;
  if (!instance || !data || size == 0 || size > sizeof(instance->pending))
    return;
  memcpy(instance->pending, data, size);
  instance->pending_size = size;
}

platform_instance_result Platform_InstanceAcquire(platform_instance **out,
                                                   const char *app_id,
                                                   int argc, char **argv) {
  (void)app_id;
  u8 wire[INSTANCE_WIRE_SIZE];
  usize wire_size = Instance_EncodeArguments(argc, argv, wire, sizeof(wire));
  HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\WorkbenchSingleInstance");
  if (!mutex)
    return WB_INSTANCE_UNAVAILABLE;

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND window = NULL;
    for (i32 attempt = 0; attempt < 200 && !window; attempt++) {
      window = FindWindowW(L"WorkbenchWindowClass", NULL);
      if (!window)
        Sleep(10);
    }
    if (window && wire_size > 0) {
      COPYDATASTRUCT data = {0};
      data.dwData = 0x57424D53; /* WBMS */
      data.cbData = (DWORD)wire_size;
      data.lpData = wire;
      SendMessageW(window, WM_COPYDATA, (WPARAM)window, (LPARAM)&data);
      CloseHandle(mutex);
      return WB_INSTANCE_FORWARDED;
    }
    CloseHandle(mutex);
    return WB_INSTANCE_UNAVAILABLE;
  }

  platform_instance *instance = calloc(1, sizeof(*instance));
  if (!instance) {
    CloseHandle(mutex);
    return WB_INSTANCE_UNAVAILABLE;
  }
  instance->mutex = mutex;
  g_platform.single_instance = instance;
  *out = instance;
  return WB_INSTANCE_PRIMARY;
}

void Platform_InstanceAttachWindow(platform_instance *instance,
                                    platform_window *window) {
  if (instance && window) {
    instance->window = window->hwnd;
    g_platform.single_instance = instance;
  }
}

b32 Platform_InstancePoll(platform_instance *instance, i32 *argc, char **argv,
                          i32 max_args, char *storage, usize storage_size) {
  if (!instance || instance->pending_size == 0)
    return false;
  usize size = instance->pending_size;
  instance->pending_size = 0;
  return Instance_DecodeArguments(instance->pending, size, argc, argv, max_args,
                                  storage, storage_size);
}

void Platform_InstanceRelease(platform_instance *instance) {
  if (!instance)
    return;
  if (g_platform.single_instance == instance)
    g_platform.single_instance = NULL;
  if (instance->mutex)
    CloseHandle(instance->mutex);
  free(instance);
}

void Platform_ActivateWindow(platform_window *window) {
  if (!window || !window->hwnd)
    return;
  ShowWindow(window->hwnd, SW_RESTORE);
  SetForegroundWindow(window->hwnd);
  BringWindowToTop(window->hwnd);
}

#else

#include "linux/linux_internal.h"
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

static void Instance_ActivationDone(
    void *data, struct xdg_activation_token_v1 *token, const char *handle) {
  platform_window *window = (platform_window *)data;
  if (window && g_platform.activation && handle && handle[0]) {
    xdg_activation_v1_activate(g_platform.activation, handle, window->surface);
    wl_display_flush(g_platform.display);
  }
  xdg_activation_token_v1_destroy(token);
}

static const struct xdg_activation_token_v1_listener activation_listener = {
    .done = Instance_ActivationDone};

struct platform_instance {
  int listener;
  int client;
  u8 pending[INSTANCE_WIRE_SIZE];
  usize pending_size;
  char address[sizeof(((struct sockaddr_un *)0)->sun_path)];
};

static void Instance_CloseClient(platform_instance *instance) {
  if (instance->client >= 0) {
    close(instance->client);
    instance->client = -1;
  }
  instance->pending_size = 0;
}

platform_instance_result Platform_InstanceAcquire(platform_instance **out,
                                                   const char *app_id,
                                                   int argc, char **argv) {
  (void)app_id;
  u8 wire[INSTANCE_WIRE_SIZE];
  usize wire_size = Instance_EncodeArguments(argc, argv, wire, sizeof(wire));
  struct sockaddr_un address = {0};
  int probe;
  int listener;

  if (!out || wire_size == 0)
    return WB_INSTANCE_UNAVAILABLE;
  address.sun_family = AF_UNIX;
  /* Abstract sockets have no stale filesystem entry after a crash. */
  address.sun_path[0] = '\0';
  snprintf(address.sun_path + 1, sizeof(address.sun_path) - 1,
           "workbench.%u", (unsigned)getuid());
  socklen_t address_size = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                       1 + strlen(address.sun_path + 1));

  probe = socket(AF_UNIX, SOCK_STREAM, 0);
  if (probe >= 0) {
    if (connect(probe, (struct sockaddr *)&address, address_size) == 0) {
      ssize_t sent = send(probe, wire, wire_size, MSG_NOSIGNAL);
      close(probe);
      return sent == (ssize_t)wire_size ? WB_INSTANCE_FORWARDED
                                        : WB_INSTANCE_UNAVAILABLE;
    }
    close(probe);
  }

  listener = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener < 0)
    return WB_INSTANCE_UNAVAILABLE;
  if (bind(listener, (struct sockaddr *)&address, address_size) != 0 ||
      listen(listener, 4) != 0) {
    close(listener);
    return WB_INSTANCE_UNAVAILABLE;
  }
  fcntl(listener, F_SETFL, O_NONBLOCK);

  platform_instance *instance = calloc(1, sizeof(*instance));
  if (!instance) {
    close(listener);
    return WB_INSTANCE_UNAVAILABLE;
  }
  instance->listener = listener;
  instance->client = -1;
  memcpy(instance->address, address.sun_path, sizeof(instance->address));
  *out = instance;
  return WB_INSTANCE_PRIMARY;
}

void Platform_InstanceAttachWindow(platform_instance *instance,
                                    platform_window *window) {
  (void)instance;
  (void)window;
}

b32 Platform_InstancePoll(platform_instance *instance, i32 *argc, char **argv,
                          i32 max_args, char *storage, usize storage_size) {
  if (!instance)
    return false;
  if (instance->client < 0) {
    instance->client = accept(instance->listener, NULL, NULL);
    if (instance->client >= 0)
      fcntl(instance->client, F_SETFL, O_NONBLOCK);
  }
  if (instance->client >= 0) {
    ssize_t received = recv(instance->client,
                            instance->pending + instance->pending_size,
                            sizeof(instance->pending) - instance->pending_size,
                            MSG_DONTWAIT);
    if (received > 0)
      instance->pending_size += (usize)received;
    if (instance->pending_size >= sizeof(u32)) {
      u32 count;
      usize expected = sizeof(count);
      memcpy(&count, instance->pending, sizeof(count));
      if (count > INSTANCE_MAX_ARGS) {
        Instance_CloseClient(instance);
      } else {
        for (u32 i = 0; i < count && expected <= instance->pending_size; i++) {
          u32 length;
          if (expected + sizeof(length) > instance->pending_size)
            break;
          memcpy(&length, instance->pending + expected, sizeof(length));
          expected += sizeof(length) + length;
        }
        if (expected <= instance->pending_size) {
          usize size = instance->pending_size;
          u8 complete[INSTANCE_WIRE_SIZE];
          memcpy(complete, instance->pending, size);
          instance->pending_size = 0;
          Instance_CloseClient(instance);
          return Instance_DecodeArguments(complete, size, argc, argv,
                                          max_args, storage, storage_size);
        }
      }
    }
    if (received == 0)
      Instance_CloseClient(instance);
  }
  return false;
}

void Platform_InstanceRelease(platform_instance *instance) {
  if (!instance)
    return;
  if (instance->client >= 0)
    close(instance->client);
  if (instance->listener >= 0)
    close(instance->listener);
  free(instance);
}

void Platform_ActivateWindow(platform_window *window) {
  if (window && g_platform.activation) {
    struct xdg_activation_token_v1 *token =
        xdg_activation_v1_get_activation_token(g_platform.activation);
    if (token) {
      xdg_activation_token_v1_set_app_id(token, "workbench");
      xdg_activation_token_v1_set_surface(token, window->surface);
      if (g_platform.seat && g_platform.last_serial)
        xdg_activation_token_v1_set_serial(token, g_platform.last_serial,
                                           g_platform.seat);
      xdg_activation_token_v1_add_listener(token, &activation_listener, window);
      xdg_activation_token_v1_commit(token);
      wl_display_flush(g_platform.display);
    }
  } else if (g_platform.display) {
    /* Older compositors simply have no activation protocol. */
    wl_display_flush(g_platform.display);
  }
}

#endif
