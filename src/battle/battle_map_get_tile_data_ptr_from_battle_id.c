#include "fft/battle.h"
#include "psx/types.h"

map_tile_t* battle_map_get_tile_data_ptr_from_battle_id(u32 battle_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_battle_id(battle_id & 0xffff);

    return battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
}
