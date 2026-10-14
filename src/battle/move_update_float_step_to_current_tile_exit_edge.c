#include "fft/battle.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/*
 * Advance a unit toward its current tile's exit edge; on reaching it, compare
 * the scaled heights of both step records and either continue into the next
 * tile or, when the next tile is more than one step higher, start the
 * 0x2000 vertical-motion path through move_start_unit_climb_jump_step_2.
 */
void move_update_float_step_to_current_tile_exit_edge(battle_unit_t* unit) {
    s32 direction;

    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        move_displace_overlapping_unit(unit, direction);
    }
    if (move_has_reached_current_tile_exit_edge(direction, unit) != 0) {
        move_snap_axis_to_current_tile_exit_edge(direction, unit);
        g_move_current_edge_height = g_move_current_tile->height * 2
            + g_move_current_tile->depth_half_height.half_height * unit->current_edge_height;
        unit->standing_on_battle_id_plus_one = 0;
        g_move_destination_edge_height = g_move_destination_tile->height * 2
            + g_move_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
        if (g_move_current_edge_height + 1 >= g_move_destination_edge_height) {
            move_start_float_step(unit, g_move_current_tile, g_move_destination_tile);
            unit->step_phase = g_move_float_to_center_phases[direction];
        } else {
            unit->step_speed = 0x2000;
            move_start_float_climb_jump_step(unit, g_move_current_tile, g_move_destination_tile);
        }
    }
}
