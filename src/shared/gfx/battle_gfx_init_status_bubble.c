#include "fft/battle_gfx.h"

/* Per-frame float and status-bubble update of one unit: adjusts its float height
 * except during the four float-jump step phases, then advances its status bubble
 * when one is active. */
void battle_gfx_init_status_bubble(battle_unit_t* unit) {
    u8 phase;

    phase = unit->step_phase;
    if (phase != MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH && phase != MOVE_STEP_PHASE_FLOAT_JUMP_EAST
        && phase != MOVE_STEP_PHASE_FLOAT_JUMP_WEST && phase != MOVE_STEP_PHASE_FLOAT_JUMP_NORTH) {
        battle_unit_increment_or_decrement_height_mod(unit);
    }
    if (unit->status_bubble_active != 0) {
        battle_gfx_determine_status_bubble_parameters(unit);
    }
}
