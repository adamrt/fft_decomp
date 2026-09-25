#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Reject the Gil<= condition when current war funds are above the operand. */
void wldcore_opcode_map_check_war_funds_at_most(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    s32 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (value < world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS)) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
