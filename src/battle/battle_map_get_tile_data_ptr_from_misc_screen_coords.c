#include "fft/battle.h"

map_tile_t* battle_map_get_tile_data_ptr_from_misc_screen_coords(u32 misc_id) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    return battle_map_get_tile_data_pointer(unit->screen.vx / 28, unit->screen.vz / 28, unit->map_z);
}
