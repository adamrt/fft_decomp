#include "fft/battle.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/* Step handler of the FLOAT_JUMP stages: advance a floating unit's jump to a
 * higher tile by one frame.
 *
 * Outside events it pushes aside a unit on the destination tile. Past the
 * destination tile's entry edge the horizontal motion stops, and once the unit is
 * down at the ground height there a float step starts into the FLOAT_TO_CENTER
 * stage of the direction. */
void move_update_unit_step_to_destination_tile_entry_edge(battle_unit_t* unit) {
    s32 direction;

    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        move_displace_unit_at_destination_tile(unit, direction);
    }
    if (move_has_reached_destination_tile_entry_edge(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        if (unit->world_coords.vy >= (s16)battle_gfx_calculate_world_height_from_world_coords(unit)) {
            unit->velocity.vy = 0;
            move_start_float_step(unit, g_move_current_tile, g_move_destination_tile);
            unit->step_phase = g_move_float_to_center_phases[direction];
        }
    }
}
