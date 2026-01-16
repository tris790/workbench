#include "wsh_repl.h"
#include "wsh_state.h"
#include <stdio.h>

int main(int argc, char **argv, char **envp) {
  /* Initialize State */
  wsh_state_t *state = WSH_State_Create(argc, argv, envp);
  if (!state) {
    fprintf(stderr, "Failed to create WSH state\n");
    return 1;
  }

  /* Run REPL */
  WSH_REPL_Run(state);

  /* Cleanup */
  WSH_State_Destroy(state);

  return 0;
}
