#include "fft/battle_unit.h"

/* Saves the unit's tile, facing, mount link and stood-on unit before it moves;
 * move_return_to_previous_map_square restores them. */
void battle_unit_save_previous_state(battle_unit_t* unit) {
    u8 x = unit->map_x;
    u8 y = unit->map_y;
    u8 z = unit->map_z;
    u16 facing = unit->facing;
    u8 mount_value = unit->mount_state;
    u8 mount_id = unit->mount_partner_runtime_id;
    u8 standing_on_battle_id_plus_one = unit->standing_on_battle_id_plus_one;

    unit->previous_map_x = x;
    unit->previous_map_y = y;
    unit->previous_map_z = z;
    unit->previous_facing = facing;
    unit->previous_mount_state = mount_value;
    unit->previous_mount_partner_runtime_id = mount_id;
    unit->previous_standing_on_battle_id_plus_one = standing_on_battle_id_plus_one;
}
