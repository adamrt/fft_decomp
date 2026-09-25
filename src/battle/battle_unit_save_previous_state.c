#include "fft/battle.h"

void battle_unit_save_previous_state(battle_unit_misc_data_t* unit) {
    u8 x = unit->map_x;
    u8 y = unit->map_y;
    u8 z = unit->map_z;
    u16 facing = unit->facing;
    u8 mount_value = unit->mount_state;
    u8 mount_id = unit->mount_partner_misc_id;
    u8 current_unit_id_plus_one = unit->current_unit_id_plus_one;

    unit->previous_map_x = x;
    unit->previous_map_y = y;
    unit->previous_map_z = z;
    unit->previous_facing = facing;
    unit->previous_mount_state = mount_value;
    unit->previous_mount_partner_misc_id = mount_id;
    unit->previous_unit_id_plus_one = current_unit_id_plus_one;
}
