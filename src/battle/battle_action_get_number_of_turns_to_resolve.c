#include "fft/battle.h"
#include "psx/types.h"

/* Count the AT-list entries that resolve before an action by unit_index
 * charging for ct; returns 0xff when no such position exists within 40. */
u32 battle_action_get_number_of_turns_to_resolve(s32 unit_index, s32 ct, battle_at_entry_t* entries) {
    s32 count = 0;
    if (g_battle_between_turn_resume_state == 9 && ct != 0) {
        ct += 1;
    }
    ct <<= 8;
    do {
        u32 entry_unit = entries->unit;
        u32 is_action = entry_unit & 0x40;
        s32 entry_ct;
        entry_unit &= 0x1F;
        if (entry_unit == 0x1F)
            return 0xFF;
        entry_ct = (s32)entries->sort_key;
        if (entry_ct >= ct) {
            if (ct != entry_ct)
                return count & 0xFF;
            if (unit_index == (s32)entry_unit && is_action != 0)
                return count & 0xFF;
            if ((s32)entry_unit >= unit_index)
                return count & 0xFF;
            if (is_action == 0)
                return count & 0xFF;
        }
        count++;
        entries++;
    } while (count < 0x28);
    return 0xFF;
}
