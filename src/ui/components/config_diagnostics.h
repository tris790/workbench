#ifndef CONFIG_DIAGNOSTICS_H
#define CONFIG_DIAGNOSTICS_H

#include "../ui.h"

#include "../layout.h"

/* Render the configuration diagnostics modal.
 * This modal displays loaded config values and any errors.
 */
void ConfigDiagnostics_Render(ui_context *ui, rect bounds,
                              layout_state *layout);

#endif /* CONFIG_DIAGNOSTICS_H */
