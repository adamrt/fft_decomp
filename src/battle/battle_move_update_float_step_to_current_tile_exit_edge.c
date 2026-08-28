#include "fft/battle.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/*
 * Advance a unit toward its current tile's exit edge; on reaching it, compare
 * the scaled heights of both step records and either continue into the next
 * tile or, when the next tile is more than one step higher, start the
 * 0x2000 vertical-motion path through battle_move_start_unit_climb_jump_step_2.
 */
void battle_move_update_float_step_to_current_tile_exit_edge(battle_unit_misc_data_t* unit) {
    s32 direction;

    direction = battle_move_get_direction(unit);
    battle_move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        battle_move_displace_overlapping_unit(unit, direction);
    }
    if (battle_move_has_reached_current_tile_exit_edge(direction, unit) != 0) {
        battle_move_snap_axis_to_current_tile_exit_edge(direction, unit);
        g_battle_move_current_edge_height = g_battle_move_current_tile->height * 2
            + (g_battle_move_current_tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) * unit->current_edge_height;
        unit->current_unit_id_plus_one = 0;
        g_battle_move_destination_edge_height = g_battle_move_destination_tile->height * 2
            + (g_battle_move_destination_tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK)
                * unit->destination_edge_height;
        if (g_battle_move_current_edge_height + 1 >= g_battle_move_destination_edge_height) {
            battle_move_start_float_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
            unit->centre_tile_offset = g_battle_move_entry_edge_centre_offsets[direction];
        } else {
            unit->step_speed = 0x2000;
            battle_move_start_float_climb_jump_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
        }
    }
}
