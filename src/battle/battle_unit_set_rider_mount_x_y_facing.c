#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_set_rider_mount_x_y_facing(battle_unit_misc_data_t* unit) {
    battle_unit_misc_data_t* mount;
    s16 rot;
    s32 unused[4];

    if (unit != 0) {
        if ((unit->status_flags_1_4 & BATTLE_MISC_STATUS_DEAD) == 0) {
            /* The facing halfwords are read signed (lh) here. */
            rot = *(s16*)&unit->attack_facing;
            if (rot != -1) {
                unit->facing = rot;
                battle_unit_set_tile_position(
                    unit->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z, rot / 1024);
                /* Stored as a signed halfword (li -1, not ori 0xffff). */
                *(s16*)&unit->attack_facing = -1;
                if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
                    mount = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
                    mount->facing = unit->facing;
                    battle_unit_set_tile_position(unit->battle_data->misc_unit_id, unit->map_x, unit->map_y,
                        unit->map_z, *(s16*)&unit->facing / 1024);
                }
            }
        } else {
            *(s16*)&unit->attack_facing = -1;
        }
    }
}
