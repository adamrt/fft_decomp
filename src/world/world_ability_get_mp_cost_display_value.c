#include "fft/world.h"
#include "psx/types.h"

/* Returns the selected action ability's displayed MP cost.
 *
 * The value comes from secondary record byte 0xd rather than the ability's
 * JP cost. */
s32 world_ability_get_mp_cost_display_value(s32 index) {
    s32 result;
    s32 entry = g_world_ability_entries[index];

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = 0x20000000;
    } else {
        result = g_world_selected_ability_secondary_data[0xd];
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= 0x40000000;
    }
    return result;
}
