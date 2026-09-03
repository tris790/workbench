#ifndef LINUX_PORTAL_H
#define LINUX_PORTAL_H

#include "../../ui/components/save_dialog.h"
#include "../../ui/ui.h"

typedef struct portal_server portal_server;

/* Install the user-level D-Bus activation and portal selection files. */
b32 Portal_Install(void);

/* Start the Workbench FileChooser backend. */
b32 Portal_Create(portal_server **out_server, memory_arena *arena);
b32 Portal_Init(portal_server *server, save_dialog_state *picker,
                ui_context *ui, platform_window *window);
void Portal_Poll(portal_server *server);
void Portal_Complete(portal_server *server);
void Portal_CancelAll(portal_server *server);
void Portal_Shutdown(portal_server *server);
void Portal_Destroy(portal_server *server);

#endif /* LINUX_PORTAL_H */
