#include "fft/event_bunit.h"
#include "psx/types.h"

u32 bunit_ability_get_mp_cost_display_value(s32 index) {
    s32 entry = g_bunit_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = 0x20000000;
    } else {
        result = ((ability_secondary_data_t*)g_bunit_ability_selected_specific)->mp_cost;
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= 0x40000000;
    }
    return result;
}
