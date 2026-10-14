#include "fft/battle_move.h"
#include "psx/types.h"

/*
 * Advance a unit's final vertical step onto its destination tile center.
 *
 * Once the tile center is reached, the target height is the tile's screen Z,
 * or 10 above the mount's when the path is complete and pending_mount bit 0x80
 * names a mount. The step ends when the unit reaches that height in its
 * direction of vertical travel (velocity.vy) or when path steps remain; it
 * then snaps to the center, clears velocity, adopts the destination map
 * coordinates and sets shadow_dirty bit 0.
 */
void move_update_unit_vertical_step_to_destination_tile_center(battle_unit_t* unit) {
    s32 done;
    s32 direction;
    battle_unit_t* mount;

    done = 0;
    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (move_has_reached_destination_tile_center(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        g_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(unit);
        if (unit->movement_path_offset == unit->path.count && (unit->path.pending_mount & BATTLE_PENDING_MOUNT_SET)) {
            mount = battle_unit_get_by_battle_id(unit->path.pending_mount & BATTLE_PENDING_MOUNT_ID_MASK);
            if (mount != 0) {
                g_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(mount) - 10;
            }
        }
        if (unit->velocity.vy > 0) {
            if (unit->world_coords.vy >= g_move_target_screen_z) {
                done = 1;
            }
        } else if (unit->world_coords.vy <= g_move_target_screen_z) {
            done = 1;
        }
        if (unit->movement_path_offset != unit->path.count) {
            done = 1;
        }
        if (done) {
            unit->velocity.vy = 0;
            move_snap_axis_to_destination_tile_center(direction, unit);
            unit->velocity.vz = 0;
            unit->velocity.vx = 0;
            g_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(unit);
            unit->step_phase = MOVE_STEP_PHASE_NONE;
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->shadow_dirty |= 1;
        }
    }
}
