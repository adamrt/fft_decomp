#include "fft/battle_move.h"
#include "psx/types.h"

/* Continue a jump step on the ground once the landing animation is over: restart
 * the step (at climb speed when the step byte's climb bit is set) and enter its
 * to-center stage for the direction of travel. */
void move_advance_display_unit_step(battle_unit_t* unit) {
    s32 direction;

    direction = move_get_direction(unit);
    switch (g_move_step_value.climb) {
    case 0:
        move_start_unit_step(unit, g_move_current_tile, g_move_destination_tile);
        unit->step_phase = g_move_walk_to_center_phases[direction];
        break;
    case 1:
        move_start_unit_step_at_climb_speed(unit, g_move_current_tile, g_move_destination_tile);
        unit->step_phase = g_move_climb_to_center_phases[direction];
        break;
    }
}
