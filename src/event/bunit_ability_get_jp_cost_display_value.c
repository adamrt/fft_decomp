#include "fft/battle_ability.h"
#include "fft/bunit.h"
#include "fft/data.h"
#include "psx/types.h"

u32 bunit_ability_get_jp_cost_display_value(s32 index) {
    s32 entry = g_bunit_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = 0x20000000;
    } else {
        result = g_main_ability_data[entry & ABILITY_LIST_ENTRY_ID_MASK].jp_cost;
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= 0x40000000;
    }
    return result;
}
