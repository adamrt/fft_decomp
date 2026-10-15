#include "fft/battle_action.h"
#include "fft/battle_unit.h"

/* The crystal or treasure lying on a tile, or 0. Among several, the source unit
 * (g_battle_unit_source_runtime_id) is preferred, then the first in list order. The
 * candidate array holds 16 entries and is filled without a bound check. */
battle_unit_t* battle_unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z) {
    battle_unit_t* matches[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    battle_unit_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_z == map_z) && (unit->stats != 0)
            && (unit->stats->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE)) != 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            if (matches[index]->runtime_id == g_battle_unit_source_runtime_id) {
                return matches[index];
            }
        }
        return matches[0];
    }
    return 0;
}
