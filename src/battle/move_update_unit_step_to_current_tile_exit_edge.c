#include "fft/battle_move.h"
#include "psx/types.h"

/* Step handler of the FLY_TO_EDGE stages: advance a flying unit one frame toward
 * the exit edge of its tile.
 *
 * At the edge it stores the edge heights of the current and destination tiles
 * (height * 2 + slope half-height * edge value), clears the unit it stood on,
 * builds the velocity for the run of contiguous steps and enters the
 * FLY_TO_CENTER stage. */
void move_update_unit_step_to_current_tile_exit_edge(battle_unit_t* unit) {
    s32 direction;

    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (move_has_reached_current_tile_exit_edge(direction, unit) != 0) {
        move_snap_axis_to_current_tile_exit_edge(direction, unit);
        g_move_current_edge_height = g_move_current_tile->height * 2
            + g_move_current_tile->depth_half_height.half_height * unit->current_edge_height;
        unit->standing_on_battle_id_plus_one = 0;
        g_move_destination_edge_height = g_move_destination_tile->height * 2
            + g_move_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
        move_set_velocity_for_contiguous_steps_with_final_tile_height(unit, &unit->path, &unit->current_path_step);
        move_interpolate_partial(unit, unit->walk_speed);
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH;
    }
}
