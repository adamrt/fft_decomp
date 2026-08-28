#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_store_coordinate_mount_animation_data(battle_unit_misc_data_t* unit) {
    battle_stats_t* data;
    battle_unit_misc_data_t* mount;

    data = unit->battle_data;
    if (data != 0) {
        /* facing is read signed (lh) here. */
        battle_unit_set_tile_position(
            data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z, *(s16*)&unit->facing / 1024);
        if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
            mount = battle_unit_get_misc_data_by_misc_id(unit->unit_id);
            if (unit->battle_data != 0) {
                battle_unit_set_tile_position(mount->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z,
                    *(s16*)&unit->facing / 1024);
            }
        }
    }
    battle_unit_set_move_and_screen_coords(unit);
    battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
    battle_target_move_cursor_to_unit(unit);
    unit->movement_path_count = 0;
    battle_unit_set_animation_based_on_status(unit);
}
