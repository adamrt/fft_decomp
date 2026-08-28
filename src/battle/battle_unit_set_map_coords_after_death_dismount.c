#include "fft/battle.h"

void battle_unit_set_map_coords_after_death_dismount(battle_unit_misc_data_t* unit) {
    /* pending_attack_result is the "Death by Dismount" word. */
    if (unit->pending_attack_result != 0) {
        battle_unit_dismount_rider_and_update_display(unit);
        unit->map_x = unit->dismount.x;
        unit->map_y = unit->dismount.y;
        unit->map_z = unit->dismount.level;
        battle_unit_set_move_and_screen_coords(unit);
        battle_unit_set_tile_position(
            unit->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z, (u8)(*(s16*)&unit->facing / 0x400));
        unit->pending_attack_result = 0;
    }
}
