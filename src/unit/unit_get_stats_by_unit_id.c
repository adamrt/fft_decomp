#include "fft/battle.h"
#include "fft/battle_unit.h"

/* Resolves an event unit id to battle stats for the scenario conditions; returns
 * -1 for none.
 *
 * An id found among the ENTD unit ids gives that unit when it has a unit record.
 * Otherwise an id in the special-character identity range is looked up by
 * character identity and accepted only for a deployed party member (unit id
 * 0x78..0x7c). */
unit_stats_t* unit_get_stats_by_unit_id(s32 unit_id) {
    s32 index;
    unit_stats_t* stats;

    battle_unit_find_stats_by_entd_unit_id(unit_id, &index);
    if (index < 0) {
        if (unit_id < CHARACTER_IDENTITY_RAMZA_CHAPTER_1 || unit_id >= CHARACTER_IDENTITY_GENERIC_FIRST) {
            return (unit_stats_t*)-1;
        }
        index = battle_unit_find_slot_by_character_identity(unit_id);
        if (index >= 0) {
            stats = battle_unit_get_stats_from_battle_id(index);
            if (stats->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && stats->unit_id < EVENT_UNIT_ID_DEPLOYED_END) {
                return stats;
            }
        }
        return (unit_stats_t*)-1;
    }
    if (battle_unit_get_runtime_id_by_battle_id(index) >= 0) {
        return battle_unit_get_stats_from_battle_id(index);
    }
    return (unit_stats_t*)-1;
}
