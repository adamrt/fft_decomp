#include "fft/battle.h"
#include "psx/types.h"

/*
 * Advance a unit's final vertical step onto its destination tile centre.
 *
 * Once the tile centre is reached, the target height is the tile's screen Z,
 * or 10 above the mount's when the path is complete and mount_byte bit 0x80
 * names a mount. The step ends when the unit reaches that height in its
 * direction of vertical travel (velocity.vy) or when path steps remain; it
 * then snaps to the centre, clears velocity, adopts the destination map
 * coordinates and sets shadow_dirty bit 0.
 */
void battle_move_update_unit_vertical_step_to_destination_tile_center(battle_unit_misc_data_t* unit) {
    s32 done;
    s32 direction;
    battle_unit_misc_data_t* mount;

    done = 0;
    direction = battle_move_get_direction(unit);
    battle_move_apply_unit_step_velocity(unit);
    if (battle_move_has_reached_destination_tile_center(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        g_battle_move_target_screen_z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
        if (unit->movement_path_offset == unit->movement_path_count && (unit->mount_byte & 0x80)) {
            mount = battle_unit_get_misc_data_by_battle_id(unit->mount_byte & 0x7f);
            if (mount != 0) {
                g_battle_move_target_screen_z = battle_gfx_calculate_screen_z_from_misc_screen_data(mount) - 10;
            }
        }
        if (unit->velocity.vy > 0) {
            if (unit->screen.vy >= g_battle_move_target_screen_z) {
                done = 1;
            }
        } else if (unit->screen.vy <= g_battle_move_target_screen_z) {
            done = 1;
        }
        if (unit->movement_path_offset != unit->movement_path_count) {
            done = 1;
        }
        if (done) {
            unit->velocity.vy = 0;
            battle_move_snap_axis_to_destination_tile_center(direction, unit);
            unit->velocity.vz = 0;
            unit->velocity.vx = 0;
            g_battle_move_target_screen_z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
            unit->centre_tile_offset = 0;
            unit->map_x = unit->movement.bytes.destination_x;
            unit->map_y = unit->movement.bytes.destination_y;
            unit->shadow_dirty |= 1;
        }
    }
}
