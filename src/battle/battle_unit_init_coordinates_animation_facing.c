#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_init_coordinates_animation_facing(battle_unit_misc_data_t* unit) {
    unit->centre_tile_offset = 0x3C;
    battle_unit_store_animation_facing(0x3C, (s16)unit->facing, unit);
    if (unit->movement_path_count == 0xFE) {
        unit->movement.bytes.destination_x = unit->movement_path[0];
        unit->movement.bytes.destination_y = unit->movement_path[1];
        unit->movement.bytes.destination_z = unit->movement_path[2];
    } else {
        unit->movement.bytes.destination_x = unit->map_x;
        unit->movement.bytes.destination_y = unit->map_y;
        unit->movement.bytes.destination_z = unit->map_z;
    }
}
