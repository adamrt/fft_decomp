#include "fft/battle.h"
#include "fft/event.h"
#include "fft/world.h"

battle_stats_t* world_unit_get_by_unit_id(s32 unit_id) {
    s32 index;
    battle_stats_t* stats;
    s32 raid_class;

    find_unit_by_id(unit_id, &index);
    if (index < 0) {
        if ((u32)(unit_id - CHARACTER_IDENTITY_RAMZA_CHAPTER_1)
            >= CHARACTER_IDENTITY_GENERIC_FIRST - CHARACTER_IDENTITY_RAMZA_CHAPTER_1) {
            return (battle_stats_t*)-1;
        }
        raid_class = battle_classify_character_identity_slot(unit_id);
        index = raid_class;
        /* Both failures here share the -1 return of the lookup below, as in
           the target; separate returns are laid out after the lookup. */
        if (raid_class < 0) {
            goto ret_neg;
        }
        stats = battle_unit_get_stats_from_battle_id(raid_class);
        if ((u32)(stats->unit_id - EVENT_UNIT_ID_DEPLOYED_FIRST)
            < EVENT_UNIT_ID_DEPLOYED_END - EVENT_UNIT_ID_DEPLOYED_FIRST) {
            return stats;
        }
        goto ret_neg;
    }
    if (battle_unit_get_misc_id_by_battle_id(index) < 0) {
    ret_neg:
        return (battle_stats_t*)-1;
    }
    return battle_unit_get_stats_from_battle_id(index);
}
