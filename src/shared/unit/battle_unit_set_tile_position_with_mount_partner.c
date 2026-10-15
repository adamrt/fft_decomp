#include "fft/battle_unit.h"

/* Stores a unit's tile and facing in its battle stats and gives its mount partner
 * the same facing, camera-relative facing and tile. Run when the wait direction
 * is confirmed. */
void battle_unit_set_tile_position_with_mount_partner(battle_unit_t* unit) {
    battle_unit_t* mounted_unit;

    battle_unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / BATTLE_FACING_QUARTER_TURN));

    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mounted_unit != 0) {
            mounted_unit->facing = unit->facing;
            mounted_unit->camera_facing_quadrant = unit->camera_facing_quadrant;
            mounted_unit->camera_facing_sixteenth = unit->camera_facing_sixteenth;

            battle_unit_set_tile_position(mounted_unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z,
                (unit->facing / BATTLE_FACING_QUARTER_TURN));
        }
    }
}
