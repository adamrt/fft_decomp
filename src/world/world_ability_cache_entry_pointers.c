#include "fft/world.h"
#include "psx/types.h"

/* Caches the ability record pointers for one ability list entry and reports
 * whether it is an action ability (entry bits 14-15 clear). */
s32 world_ability_cache_entry_pointers(s32 index) {
    s32 result;

    g_world_ability_selected_type
        = main_ability_calculate_pointers_and_type(((u16*)g_world_ability_entries)[index] & ABILITY_LIST_ENTRY_ID_MASK,
            &g_world_selected_ability_data, &g_world_selected_ability_secondary_data);
    result = 0;
    if (g_world_ability_selected_type != 0) {
        result = (((u16*)g_world_ability_entries)[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) == 0;
    }
    return result;
}
