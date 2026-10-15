#include "fft/battle_unit.h"
#include "psx/types.h"

/* Turns a living unit back to the facing saved in attack_facing (-1 for none) and
 * clears the saved value; a dead unit only has it cleared.
 *
 * The battle stats take the facing in quarter turns through
 * battle_unit_set_tile_position. For a mounted pair the partner record's facing
 * is copied too, without a null test, and the tile position is stored a second
 * time under the unit's own id. */
void battle_unit_restore_facing_after_attack(battle_unit_t* unit) {
    battle_unit_t* mount;
    s16 rot;
    s32 unused[4];

    if (unit != 0) {
        if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) == 0) {
            /* The facing 16-bit values are read signed (lh) here. */
            rot = unit->attack_facing;
            if (rot != -1) {
                unit->facing = rot;
                battle_unit_set_tile_position(
                    unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, rot / 1024);
                /* Stored as a signed 16-bit value (li -1, not ori 0xffff). */
                unit->attack_facing = -1;
                if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
                    mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                    mount->facing = unit->facing;
                    battle_unit_set_tile_position(
                        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / 1024);
                }
            }
        } else {
            unit->attack_facing = -1;
        }
    }
}
