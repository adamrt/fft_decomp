#include "fft/battle.h"

/*
 * Claim a Misc Unit Data slot and stamp its unit id.
 *
 * Ids below 16 take their own slot, failing if another in-use slot already
 * carries that id or the slot is taken. Larger ids take the first free slot,
 * whose index becomes the stored id; exhausting all 16 raises animation error
 * 6. Returns 0 on failure.
 */
battle_unit_misc_data_t* battle_unit_claim_misc_slot(s16 unit_id) {
    s32 i;

    if (unit_id < 16) {
        for (i = 0; i < 16; i++) {
            if (g_battle_unit_misc_slot_flags[i].in_use != 0 && g_battle_unit_misc_data[i].unit_id == unit_id) {
                break;
            }
        }
        if (i < 16) {
            return 0;
        }
        if (g_battle_unit_misc_slot_flags[unit_id].in_use != 0) {
            return 0;
        }
        g_battle_unit_misc_slot_flags[unit_id].in_use = 1;
        g_battle_unit_misc_data[unit_id].unit_id = unit_id;
        return &g_battle_unit_misc_data[unit_id];
    }
    for (i = 0; i < 16; i++) {
        if (g_battle_unit_misc_slot_flags[i].in_use == 0) {
            g_battle_unit_misc_slot_flags[i].in_use = 1;
            g_battle_unit_misc_data[i].unit_id = i;
            return &g_battle_unit_misc_data[i];
        }
    }
    main_system_handle_animation_exception(6);
    return 0;
}
