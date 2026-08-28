#include "fft/data.h"
#include "fft/jobstts.h"
#include "psx/types.h"

/* Same target slot; the alias preserves independent a1/a2 materialization. */

s32 jobstts_ability_is_non_action_learned(s32 index) {
    u16* entries;
    u16* entry;
    s32 result;
    u16 ability;

    entries = (u16*)g_jobstts_ability_entries;
    entry = &entries[index];
    g_jobstts_ability_type = main_ability_calculate_pointers_and_type(*entry & ABILITY_LIST_ENTRY_ID_MASK,
        &g_jobstts_ability_selected_specific, &g_jobstts_ability_selected_specific_arg_alias);
    result = 0;
    if (g_jobstts_ability_type != ABILITY_TYPE_DEFAULT) {
        ability = *entry;
        result = (ability >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) == 0;
    }
    return result;
}
