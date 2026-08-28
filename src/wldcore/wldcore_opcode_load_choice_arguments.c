#include "fft/wldcore.h"
#include "psx/types.h"

/* Stage four variable/value pairs for a deferred Choice request. */
void wldcore_opcode_load_choice_arguments(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        g_wldcore_script_state.choice_vars[i] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
        g_wldcore_script_state.args[i] = g_wldcore_script_state.data[++g_wldcore_script_state.ip];
        g_wldcore_script_state.ip++;
    }
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_CHOICE;
}
