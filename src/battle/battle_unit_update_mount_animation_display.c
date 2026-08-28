#include "fft/battle.h"

void battle_unit_update_mount_animation_display(battle_unit_misc_data_t* unit) {
    battle_unit_misc_data_t* mounted_unit;

    battle_unit_set_tile_position(
        unit->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z, (u8)(*(s16*)&unit->facing / 0x400));

    if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
        mounted_unit = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (mounted_unit != 0) {
            mounted_unit->facing = unit->facing;
            mounted_unit->camera_facing_quadrant.u = unit->camera_facing_quadrant.u;
            mounted_unit->camera_facing_sixteenth.u = unit->camera_facing_sixteenth.u;

            battle_unit_set_tile_position(mounted_unit->battle_data->misc_unit_id, unit->map_x, unit->map_y,
                unit->map_z, (u8)(*(s16*)&unit->facing / 0x400));
        }
    }
}
