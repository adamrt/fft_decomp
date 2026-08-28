#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Reject the Casualties>= condition when the current count is below the operand. */
void wldcore_opcode_map_check_casualties_at_least(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    u16 value = ((u16*)g_wldcore_script_state.data)[idx];
    *ip = idx + 1;
    if (world_script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) < (s32)value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
