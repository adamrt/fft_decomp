#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

/* Returns the selected action ability's displayed charge time.
 *
 * The value is ceil(100 / secondary record byte 0xc). The result is
 * accumulated in the index parameter's register to preserve allocation. */
s32 world_ability_get_ct_display_value(s32 index) {
    s32 entry = g_world_ability_entries[index];

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        index = 0x20000000;
    } else {
        index = g_world_selected_ability_secondary_data[0xc];
        index = (100 / index) + ((100 % index) != 0);
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        index |= 0x40000000;
    }
    return index;
}
