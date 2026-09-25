#include "fft/wldcore.h"
#include "psx/types.h"

/* Reject the Casualties<= condition when the current count is above the operand. */
void wldcore_opcode_map_check_casualties_at_most(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    s32 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (value < world_script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES)) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
