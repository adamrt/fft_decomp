#include "fft/wldcore.h"
#include "psx/types.h"

/* Stage the location ID for a deferred DrawLocation request. */
void wldcore_opcode_load_draw_location_argument(void) {
    g_wldcore_script_flags |= WLDCORE_SCRIPT_REQUEST_DRAW_LOCATION;
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
}
