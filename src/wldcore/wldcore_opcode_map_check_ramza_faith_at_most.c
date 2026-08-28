#include "fft/wldcore.h"

/* Reject the MCFaith<= condition when Ramza's Faith is above the operand. */
void wldcore_opcode_map_check_ramza_faith_at_most(void) {
    party_data_t* party;
    u16* ipp;
    u16 ip;
    s32 val;

    party = wldcore_get_party_data_pointer(wldcore_get_ramza_s_roster_index());
    ipp = &g_wldcore_script_state.ip;
    ip = *ipp;
    val = g_wldcore_script_state.data[ip];
    *ipp = ip + 1;
    if (val < (s32)party->faith) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
