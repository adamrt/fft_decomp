#include "fft/wldcore.h"

/* Reject the HasUnit condition when no roster unit has the operand's sprite set. */
void wldcore_opcode_map_check_roster_has_sprite_set(void) {
    s32 i;
    s32 sprite_set;
    u16* ipp;
    u16 ip;
    party_data_t* unit;
    s32* flags;

    ipp = &g_wldcore_script_state.ip;
    ip = *ipp;
    sprite_set = g_wldcore_script_state.data[ip];
    *ipp = ip + 1;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = wldcore_get_party_data_pointer(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->sprite_set == sprite_set) {
                break;
            }
        }
    }
    if (i == PARTY_ROSTER_SLOT_COUNT) {
        flags = &g_wldcore_script_state.flags;
        *flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
