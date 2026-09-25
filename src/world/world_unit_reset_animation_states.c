#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Reset the per-unit facing/animation records for all 21 simulation slots and
 * reseed each unit's facing from its misc data, or 0xff when the slot is
 * empty. */
void world_unit_reset_animation_states(void) {
    world_unit_animation_state_t* state;
    s32 i;

    i = 0;
    state = g_world_unit_animation_states;
    do {
        state->rotating = 0;
        state->delay = 0;
        if (battle_unit_has_misc_id(i) != 0) {
            state->initial_facing = battle_unit_get_facing_nibble_by_misc_id(i);
        } else {
            state->initial_facing = 0xff;
        }
        g_world_unit_pending_animations[i] = 0;
        g_world_unit_was_moving_latch[i] = 0;
        i++;
        state++;
    } while (i < BATTLE_UNIT_SLOT_COUNT);
}
