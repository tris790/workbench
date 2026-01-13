#ifndef APP_ARGS_H
#define APP_ARGS_H

#include "core/args.h"
#include "ui/layout.h"

/* Apply parsed arguments to the application layout state */
void Args_Handle(layout_state *layout, const app_args *args);

#endif /* APP_ARGS_H */
