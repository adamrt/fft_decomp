#include "fft/battle.h"

battle_unit_misc_data_t* battle_unit_find_any_misc_data_at_map_coords(s32 map_x, s32 map_y, s32 map_z) {
    battle_unit_misc_data_t* unit;

    unit = g_battle_unit_misc_list_head;
    while (unit != 0) {
        if (unit->map_x == map_x && unit->map_y == map_y && unit->map_z == map_z) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}
