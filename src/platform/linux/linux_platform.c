/*
 * linux_platform.c - Linux platform initialization and cleanup
 */

#include "linux_internal.h"

linux_platform g_platform = {0};

/* ===== Registry Callbacks ===== */

static void RegistryGlobal(void *data, struct wl_registry *registry, u32 name,
                           const char *interface, u32 version) {
  (void)data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    g_platform.compositor = wl_registry_bind(
        registry, name, &wl_compositor_interface, Min(version, 4));
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    g_platform.shm =
        wl_registry_bind(registry, name, &wl_shm_interface, Min(version, 1));
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    g_platform.xdg_wm_base = wl_registry_bind(
        registry, name, &xdg_wm_base_interface, Min(version, 2));
    /* Listener will be added in Init or here if we expose the listener */
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    g_platform.seat =
        wl_registry_bind(registry, name, &wl_seat_interface, Min(version, 5));
    /* Seat listener added in events module usually, but we need to ensure it's hooked up.
     * For simplicity, we'll keep the seat listener in linux_events.c and we might need to 
     * call a registration function or extern the listener. 
     * 
     * Actually, better approach: define listeners in their respective modules and extern them 
     * or register them here.
     * 
     * Let's include the headers or extern them.
     */
  } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) ==
             0) {
    g_platform.decoration_manager = wl_registry_bind(
        registry, name, &zxdg_decoration_manager_v1_interface, Min(version, 1));
  } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
    g_platform.data_device_manager = wl_registry_bind(
        registry, name, &wl_data_device_manager_interface, Min(version, 3));
  }
}

static void RegistryGlobalRemove(void *data, struct wl_registry *registry,
                                 u32 name) {
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = RegistryGlobal,
    .global_remove = RegistryGlobalRemove,
};

/* ===== XDG WM Base Listener ===== */
/* Needed for ping/pong during Init */

static void XdgWmBasePing(void *data, struct xdg_wm_base *xdg_wm_base,
                          u32 serial) {
  (void)data;
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = XdgWmBasePing,
};

/* ===== Seat & Data Device Listeners ===== */
/* These are defined in linux_events.c and linux_clipboard.c but we need to hook them up.
 * Use forward declarations or externs.
 */
extern const struct wl_seat_listener seat_listener;
extern const struct wl_data_device_listener data_device_listener;

/* ===== Platform Init/Shutdown ===== */

b32 Platform_Init(void) {
  if (g_platform.initialized)
    return true;

  g_platform.display = wl_display_connect(NULL);
  if (!g_platform.display) {
    fprintf(stderr, "Failed to connect to Wayland display\n");
    return false;
  }

  g_platform.registry = wl_display_get_registry(g_platform.display);
  wl_registry_add_listener(g_platform.registry, &registry_listener, NULL);

  wl_display_roundtrip(g_platform.display);

  /* Hook up listeners that were bound in registry roundtrip */
  if (g_platform.xdg_wm_base) {
      xdg_wm_base_add_listener(g_platform.xdg_wm_base, &xdg_wm_base_listener, NULL);
  }
  
  if (g_platform.seat) {
      wl_seat_add_listener(g_platform.seat, &seat_listener, NULL);
  }

  if (g_platform.seat && g_platform.data_device_manager) {
    g_platform.data_device = wl_data_device_manager_get_data_device(
        g_platform.data_device_manager, g_platform.seat);
    wl_data_device_add_listener(g_platform.data_device, &data_device_listener,
                                NULL);
  }

  if (!g_platform.compositor || !g_platform.xdg_wm_base || !g_platform.shm) {
    fprintf(stderr, "Missing required Wayland interfaces\n");
    return false;
  }

  g_platform.initialized = true;
  return true;
}

void Platform_Shutdown(void) {
  if (!g_platform.initialized)
    return;

  if (g_platform.clipboard_content)
    free(g_platform.clipboard_content);
  if (g_platform.clipboard_source)
    wl_data_source_destroy(g_platform.clipboard_source);
  if (g_platform.selection_offer)
    wl_data_offer_destroy(g_platform.selection_offer);
  if (g_platform.data_device)
    wl_data_device_destroy(g_platform.data_device);
  if (g_platform.data_device_manager)
    wl_data_device_manager_destroy(g_platform.data_device_manager);

  if (g_platform.keyboard)
    wl_keyboard_destroy(g_platform.keyboard);
  if (g_platform.pointer)
    wl_pointer_destroy(g_platform.pointer);
  if (g_platform.seat)
    wl_seat_destroy(g_platform.seat);
  if (g_platform.xdg_wm_base)
    xdg_wm_base_destroy(g_platform.xdg_wm_base);
  if (g_platform.decoration_manager)
    zxdg_decoration_manager_v1_destroy(g_platform.decoration_manager);
  if (g_platform.shm)
    wl_shm_destroy(g_platform.shm);
  if (g_platform.compositor)
    wl_compositor_destroy(g_platform.compositor);
  if (g_platform.registry)
    wl_registry_destroy(g_platform.registry);
  if (g_platform.display)
    wl_display_disconnect(g_platform.display);

  memset(&g_platform, 0, sizeof(g_platform));

  memset(&g_platform, 0, sizeof(g_platform));
}
