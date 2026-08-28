#include "fft/battle.h"
#include "psx/types.h"

/* Rebuild the AT list and return the unit id of the chosen entry.
 *
 * Returns -4 for an empty entry; a charged-action entry (bit 0x40) returns its
 * unit id plus 0x100. */
s32 battle_action_calculate_at_list_and_get_specific_unit_id(s32 at_index) {
    u8 at_list[0xA0];
    u32 entry;
    u32 unit_id;

    battle_action_calculate_at_list((battle_at_entry_t*)at_list, 0);
    entry = at_list[at_index * 4];
    unit_id = entry & 0x1F;
    if (unit_id == 0x1F) {
        return -4;
    }
    if (entry & 0x40) {
        unit_id += 0x100;
    }
    return unit_id;
}
