#include "fft/main_unit.h"

/* 0x80066204: scratch list of a skillset's usable ability ids. */
extern u16 g_main_ability_temp_list[SKILLSET_ABILITY_LIST_COUNT];

/* Copies the skillset's ability ids into g_main_ability_temp_list, keeping only the
 * classes selected by skillset_ability_filter_e. */
u16* main_ability_store_skillset_abilities(s32 skillset, s32 flags) {
    s32 count;
    s32 i;
    s32 id;
    s32 keep;

    if (skillset >= 0x100) {
        skillset = 0;
    }
    count = 0;
    i = 0;
    do {
        id = main_ability_get_id_from_skillset(skillset, i);
        if ((u32)(id & 0xFFFF) < ABILITY_ID_REACTION_FIRST) {
            keep = flags & SKILLSET_ABILITY_FILTER_ACTION;
        } else if ((u32)(id & 0xFFFF) < ABILITY_ID_SUPPORT_FIRST) {
            keep = flags & SKILLSET_ABILITY_FILTER_REACTION;
        } else if ((u32)(id & 0xFFFF) < ABILITY_ID_MOVEMENT_FIRST + 1) {
            /* One past ABILITY_ID_MOVEMENT_FIRST in the original. */
            keep = flags & SKILLSET_ABILITY_FILTER_SUPPORT;
        } else {
            keep = flags & SKILLSET_ABILITY_FILTER_MOVEMENT;
        }
        if (keep == 0) {
            id = 0;
        }
        g_main_ability_temp_list[count++] = id;
        i += 1;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);

    while (count < SKILLSET_ABILITY_LIST_COUNT) {
        g_main_ability_temp_list[count] = 0;
        count += 1;
    }
    return g_main_ability_temp_list;
}
