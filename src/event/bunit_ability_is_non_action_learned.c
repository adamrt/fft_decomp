#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_ability_is_non_action_learned(s32 index) {
    u16* entries;
    u16* entry;
    s32 result;
    u16 ability;

    entries = (u16*)g_bunit_ability_entries;
    entry = &entries[index];
    g_bunit_ability_type = main_ability_calculate_pointers_and_type(
        *entry & ABILITY_LIST_ENTRY_ID_MASK, &g_bunit_ability_selected_data_1, &g_bunit_ability_selected_specific);
    result = 0;
    if (g_bunit_ability_type != ABILITY_TYPE_DEFAULT) {
        ability = *entry;
        result = (ability >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) == 0;
    }
    return result;
}
