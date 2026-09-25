#include "fft/battle.h"
#include "psx/types.h"

/*
 * Advance a knocked-back or path-moving unit by one frame.
 *
 * While a step is in progress (centre_tile_offset non-zero) the walk speed
 * decays by a quarter towards 0x1000 and the step phase handler runs. Once
 * no step is active, the next path byte starts a new step, or the mount
 * animation coordinates are stored when the path is exhausted.
 */
void battle_move_update_knockback_step(battle_unit_misc_data_t* unit) {
    u32 offset;
    s32 speed;

    battle_move_get_current_and_destination_tiles(unit, &g_battle_move_current_tile, &g_battle_move_destination_tile);
    g_battle_move_step_value = unit->movement_value;
    if (unit->centre_tile_offset != 0) {
        speed = unit->walk_speed.word;
        unit->walk_speed.word = (speed > ONE) ? speed / 4 * 3 : ONE;
        switch (unit->centre_tile_offset) {
        case 2:
        case 4:
        case 6:
        case 8:
            battle_move_update_unit_step_to_destination_tile_center(unit);
            break;
        case 1:
        case 3:
        case 5:
        case 7:
            battle_move_update_walking_step_at_tile_edge(unit);
            break;
        case 0x14:
        case 0x18:
        case 0x1C:
        case 0x20:
            battle_move_update_knockback_after_animation(unit);
            break;
        case 0x12:
        case 0x16:
        case 0x1A:
        case 0x1E:
            battle_move_update_airborne_ascent_phase(unit);
            /* fallthrough */
        case 0x13:
        case 0x17:
        case 0x1B:
        case 0x1F:
            battle_move_finish_unit_step_at_tile_edge(unit);
            break;
        }
        if (unit->centre_tile_offset != 0) {
            return;
        }
    }
    if (unit->movement_path_count != 0) {
        offset = unit->movement_path_offset;
        if (offset < unit->movement_path_count) {
            unit->movement_path_offset = offset + 1;
            unit->movement_value = g_battle_move_step_value = unit->movement_path[offset];
            battle_move_set_unit_step_slope_scales(unit);
            unit->step_speed = 0x2000;
            /* The target passes the destination tile where the callee declares facing. */
            ((void (*)(battle_unit_misc_data_t*, const map_tile_t*, const map_tile_t*))battle_move_start_unit_step)(
                unit, g_battle_move_current_tile, g_battle_move_destination_tile);
        } else {
            battle_unit_store_coordinate_mount_animation_data(unit);
        }
    }
}
