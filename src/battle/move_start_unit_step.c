#include "fft/battle.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/* Start the unit's next movement step from the current tile center.
 *
 * Variant of move_start_unit_step_and_dismount_rider without the rider dismount: the step byte's top
 * two bits pick the direction, which sets the facing and edge offset; its low
 * two bits extend the destination tile coordinate. The switch has no default, so `facing` is unset
 * for a direction outside 0-3 (none exists), and game state 0x2d leaves the unit's facing unchanged. */
void move_start_unit_step(battle_unit_t* unit, const map_tile_t* from, const map_tile_t* unused_to) {
    move_step_t* step;
    move_step_t current_step;
    s32 edge_height;
    s16 facing;
    u8 next;

    edge_height = unit->current_edge_height;
    step = &unit->current_path_step;
    /* The byte copy prevents a wider direction load. */
    current_step = unit->current_path_step;
    switch (current_step.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        facing = BATTLE_FACING_EAST;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_EAST;
        next = unit->map_x + 1;
        unit->movement.destination_x = next + unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        facing = BATTLE_FACING_WEST;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_WEST;
        next = unit->map_x - 1;
        unit->movement.destination_x = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        facing = BATTLE_FACING_SOUTH;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH;
        next = unit->map_y - 1;
        unit->movement.destination_y = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        facing = BATTLE_FACING_NORTH;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH;
        next = unit->map_y + 1;
        unit->movement.destination_y = next + unit->current_path_step.length;
        break;
    }
    if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
        unit->facing = facing;
        battle_unit_set_idle_animation_for_movement(unit);
    }
    move_set_unit_step_delta_center_to_edge(unit, step, from, edge_height);
    move_interpolate_partial(unit, unit->walk_speed);
    unit->movement.destination_z = step->higher;
}
