#include "fft/battle.h"

battle_unit_misc_data_t* battle_unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z) {
    battle_unit_misc_data_t* matches[16];
    battle_unit_misc_data_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_battle_unit_misc_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_z == map_z) && (unit->battle_data != 0)
            && (unit->battle_data->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_flags_5_6 & (BATTLE_MISC_STATUS_CRYSTAL | BATTLE_MISC_STATUS_TREASURE)) != 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            if (matches[index]->unit_id == g_casting_unit_misc_id) {
                return matches[index];
            }
        }
        return matches[0];
    }
    return 0;
}
